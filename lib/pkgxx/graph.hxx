#pragma once

#include <deque>
#include <exception>
#include <functional>
#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

namespace pkgxx {
    namespace detail {
        // std::unwrap_ref_decay is a C++20 thing and we can't use it atm.
        template <typename T>
        struct unwrap_reference {
            using type = T;
        };
        template <typename T>
        struct unwrap_reference<std::reference_wrapper<T>> {
            using type = T&;
        };

        template <typename T>
        struct unwrap_ref_decay: unwrap_reference<std::decay_t<T>> {};

        template <typename T>
        using unwrap_ref_decay_t = typename unwrap_ref_decay<T>::type;
    }

    /** A cycle has been detected while tsorting a graph. */
    template <typename T>
    struct not_a_dag: std::runtime_error {
#if !defined(DOXYGEN)
        not_a_dag(std::vector<T>&& cycle_)
            : std::runtime_error("")
            , _cycle(std::move(cycle_)) {}

        virtual char const*
        what() const noexcept override;
#endif

    private:
        std::vector<T> _cycle;
        std::optional<std::string> _msg;
    };

    /** A directed graph that is barely enough for topological sorting. The
     * type \c T is the type of vertex and needs to be copy-constructible,
     * totally ordered, and outputtable. Instances with \c IsBidirectional
     * being \c true have slightly more overhead but supports additional
     * operations.
     */
    template <typename T,
              bool IsBidirectional = false>
    struct graph {
        /** The type of <tt>std::reference_wrapper<T const></tt>, works
         * even when T is itself a reference wrapper.
         */
        using cref_wrapper_type =
            std::reference_wrapper<
                std::decay_t<
                    detail::unwrap_ref_decay_t<T>
                    > const
            >;

        /** Add a vertex to the graph if it doesn't already exist. */
        void
        add_vertex(T const& v) {
            add_vertex_impl(v);
        }

        /** Remove a vertex from the graph if it exists. Only available for
         * bidirectional graphs. */
        std::enable_if_t<IsBidirectional, void>
        remove_vertex(T const& v);

        /** Add an edge between two vertices to the graph. The vertices
         * don't need to be added prior to calling this method.
         */
        void
        add_edge(T const& src, T const& dest);

        /** Remove an edge from the graph if it exists. Only available for
         * bidirectional graphs. */
        std::enable_if_t<IsBidirectional, void>
        remove_edge(T const& src, T const& dest);

        /** See if the graph has the given vertex. */
        bool
        has_vertex(T const& v) const {
            return _vertex_id_of.count(v) != 0;
        }

        /** Compute the shortest path between two vertices, if such a path
         * exists. */
        std::optional<std::vector<cref_wrapper_type>>
        shortest_path(T const& src, T const& dest) const;

        /** Perform a topological sort on the graph. Vertices that have no
         * out-edges will appear first. If it has a cycle \ref not_a_dag
         * will be thrown.
         */
        std::vector<cref_wrapper_type>
        tsort() const;

    private:
        using vertex_id = unsigned long;

        template <bool IsBidi, typename = void>
        struct vertex {
            vertex(T const& value_)
                : value(&value_) {}

            std::set<vertex_id> outs;
            T const* value;
        };

        template <bool IsBidi>
        struct vertex<IsBidi, std::enable_if_t<IsBidi>>: public vertex<false> {
            using vertex<false>::vertex;

            std::set<vertex_id> ins;
        };

        enum class colour {
            white, // unvisited vertices
            grey,  // visited but has unvisited edges
            black  // visited and has no unvisited edges
        };

        vertex_id
        add_vertex_impl(T const& v);

        std::optional<std::vector<cref_wrapper_type>>
        shortest_path_impl(vertex_id const& src, vertex_id const& dest) const;

        std::map<T, vertex_id> _vertex_id_of;
        std::map<vertex_id, vertex<IsBidirectional>> _vertices;
    };

    // Implementation

#if !defined(DOXYGEN)
    template <typename T>
    char const*
    not_a_dag<T>::what() const noexcept {
        if (!_msg) {
            std::stringstream ss;
            bool is_first = true;

            ss << "found a cycle: ";
            for (auto const& value: _cycle) {
                if (is_first) {
                    is_first = false;
                }
                else {
                    ss << " -> ";
                }
                ss << value;
            }
        }
        return _msg->c_str();
    }
#endif

    template <typename T, bool IsBidirectional>
    typename graph<T, IsBidirectional>::vertex_id
    graph<T, IsBidirectional>::add_vertex_impl(T const& value) {
        auto&& [it, emplaced] = _vertex_id_of.try_emplace(value, _vertex_id_of.size());
        if (emplaced) {
            _vertices.try_emplace(it->second, it->first);
        }
        return it->second;
    }

    template <typename T, bool IsBidirectional>
    std::enable_if_t<IsBidirectional, void>
    graph<T, IsBidirectional>::remove_vertex(T const& v) {
        if (auto id = _vertex_id_of.find(v); id != _vertex_id_of.end()) {
            auto v = _vertices.find(id->second);
            assert(v != _vertices.end());

            for (vertex_id out_id: v->second.outs) {
                auto out_v = _vertices.find(out_id);
                assert(out_v != _vertices.end());
                out_v->second.ins.erase(id->second);
            }

            for (vertex_id in_id: v->second.ins) {
                auto in_v = _vertices.find(in_id);
                assert(in_v != _vertices.end());
                in_v->second.outs.erase(id->second);
            }

            _vertices.erase(id->second);
            _vertex_id_of.erase(v);
        }
    }

    template <typename T, bool IsBidirectional>
    void
    graph<T, IsBidirectional>::add_edge(T const& src, T const& dest) {
        vertex_id const src_id  = add_vertex_impl(src);
        vertex_id const dest_id = add_vertex_impl(dest);

        auto sv = _vertices.find(src_id);
        assert(sv != _vertices.end());
        sv->second.outs.insert(dest_id);

        if constexpr (IsBidirectional) {
            auto dv = _vertices.find(dest_id);
            assert(dv != _vertices.end());
            dv->second.ins.insert(src_id);
        }
    }

    template <typename T, bool IsBidirectional>
    std::enable_if_t<IsBidirectional, void>
    graph<T, IsBidirectional>::remove_edge(T const& src, T const& dest) {
        if (auto id = _vertex_id_of.find(src); id != _vertex_id_of.end()) {
            auto it = _vertices.find(id->second);
            assert(it != _vertices.end());
            it->second.outs.erase(id->second);
        }
        if (auto id = _vertex_id_of.find(dest); id != _vertex_id_of.end()) {
            auto it = _vertices.find(id->second);
            assert(it != _vertices.end());
            it->second.ins.erase(id->second);
        }
    }

    template <typename T, bool IsBidirectional>
    std::vector<typename graph<T, IsBidirectional>::cref_wrapper_type>
    graph<T, IsBidirectional>::tsort() const {
        std::map<vertex_id, colour> visited;
        std::vector<cref_wrapper_type> tsorted;

        for (auto const& [id, _v]: _vertices) {
            visited[id] = colour::white;
        }

        auto const go =
            [&](auto const& self, vertex_id id, vertex<IsBidirectional> const& v) -> void {
                auto c = visited.find(id);
                assert(c != visited.end());

                if (c->second == colour::white) {
                    c->second = colour::grey;
                }
                else {
                    return; // Already visited.
                }

                for (vertex_id out: v.outs) {
                    auto out_c = visited.find(out);
                    assert(out_c != visited.end());

                    switch (out_c->second) {
                    case colour::white:
                        {
                            auto it = _vertices.find(out);
                            assert(it != _vertices.end());
                            // This is a recursive DFS. Maybe we should
                            // turn this into a non-recursive one?
                            self(self, out, it->second);
                        }
                        break;

                    case colour::grey:
                        // The edge "id" -> "out" forms a cycle, which
                        // means there must be a path going from "out" all
                        // the way back to "id". Find it using BFS and
                        // raise an exception.
                        {
                            std::vector<T> cycle;
                            for (auto value: shortest_path_impl(out, id).value()) {
                                cycle.push_back(value);
                            }
                            auto it = _vertices.find(out);
                            assert(it != _vertices.end());
                            cycle.push_back(*(it->second.value));
                            throw not_a_dag(std::move(cycle));
                        }
                        break;

                    case colour::black:
                        // The node has already been visited but this is
                        // definitely not a cycle. No need to do anything.
                        break;
                    }
                }

                if (c->second == colour::grey) {
                    tsorted.push_back(std::cref(*(v.value)));
                    visited[id] = colour::black;
                }
            };

        for (auto const& [id, v]: _vertices) {
            go(go, id, v);
        }

        return tsorted;
    }

    template <typename T, bool IsBidirectional>
    std::optional<std::vector<typename graph<T, IsBidirectional>::cref_wrapper_type>>
    graph<T, IsBidirectional>::shortest_path(T const& src, T const& dest) const {
        auto src_id  = _vertex_id_of.find(src);
        auto dest_id = _vertex_id_of.find(dest);

        if (src_id != _vertex_id_of.end() && dest_id != _vertex_id_of.end()) {
            return shortest_path_impl(src_id->second, dest_id->second);
        }
        else {
            return std::nullopt;
        }
    }

    template <typename T, bool IsBidirectional>
    std::optional<std::vector<typename graph<T, IsBidirectional>::cref_wrapper_type>>
    graph<T, IsBidirectional>::shortest_path_impl(vertex_id const& src, vertex_id const& dest) const {
        std::map<vertex_id, colour> visited;
        std::vector<cref_wrapper_type> path;
        std::deque<vertex_id> queue;

        for (auto const& [id, _v]: _vertices) {
            visited[id] = (id == src) ? colour::grey : colour::white;
        }

        auto const& go =
            [&](vertex<IsBidirectional> const& v) -> bool {
                for (vertex_id out: v.outs) {
                    if (out == dest) {
                        // Found the final destination.
                        path.push_back(std::cref(*(v.value)));
                        return true;
                    }

                    auto c = visited.find(out);
                    assert(c != visited.end());
                    if (c->second == colour::white) {
                        c->second = colour::grey;
                        queue.push_back(out);
                    }
                }
                return false;
            };

        auto src_v = _vertices.find(src);
        assert(src_v != _vertices.end());
        path.push_back(std::cref(*(src_v->second.value)));

        if (go(src_v->second)) {
            return std::move(path);
        }

        while (!queue.empty()) {
            auto id = queue.front();
            queue.pop_front();

            auto it = _vertices.find(id);
            assert(it != _vertices.end());

            if (go(it->second)) {
                return std::move(path);
            }
        }

        return std::nullopt;
    }
}

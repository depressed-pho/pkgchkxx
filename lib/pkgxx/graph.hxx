#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <cassert>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
#include <vector>

namespace pkgxx {
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
     * totally ordered, and outputtable.
     */
    template <typename T>
    struct graph {
        /** Add a vertex to the graph if it doesn't already exist. */
        void
        add_vertex(T const& v) {
            add_vertex_impl(v);
        }

        /** Add an edge between two vertices to the graph. The vertices
         * don't need to be added prior to calling this method.
         */
        void
        add_edge(T const& src, T const& dest);

        /** See if the graph has the given vertex. */
        bool
        has_vertex(T const& v) const {
            return _vertex_id_of.count(v) != 0;
        }

        /** Perform a topological sort on the graph. Vertices that have no
         * out-edges will appear on the last. If it has a cycle \ref
         * not_a_dag will be thrown.
         */
        std::vector<std::reference_wrapper<T const>>
        tsort() const;

    private:
        using vertex_id = unsigned long;

        struct vertex {
            vertex(T const& value_)
                : value(&value_) {}

            std::set<vertex_id> outs;
            T const* value;
        };

        enum class colour {
            grey,  // visited but has unvisited edges
            black  // visited and has no unvisited edges
        };

        vertex_id
        add_vertex_impl(T const& v);

        std::map<T, vertex_id> _vertex_id_of;
        std::map<vertex_id, vertex> _vertices;
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

    template <typename T>
    typename graph<T>::vertex_id
    graph<T>::add_vertex_impl(T const& value) {
        auto&& [it, emplaced] = _vertex_id_of.try_emplace(value, _vertex_id_of.size());
        if (emplaced) {
            _vertices.try_emplace(it->second, it->first);
        }
        return it->second;
    }

    template <typename T>
    void
    graph<T>::add_edge(T const& src, T const& dest) {
        vertex_id const src_id  = add_vertex_impl(src);
        vertex_id const dest_id = add_vertex_impl(dest);

        auto it = _vertices.find(src_id);
        assert(it != _vertices.end());
        it->second.outs.insert(dest_id);
    }

    template <typename T>
    std::vector<std::reference_wrapper<T const>>
    graph<T>::tsort() const {
        std::map<vertex_id, colour> visited;
        std::vector<vertex_id> stack;
        std::vector<std::reference_wrapper<T const>> tsorted;

        auto const go =
            [&](auto const& self, vertex_id id, vertex const& v) -> void {
                auto const depth = stack.size();

                auto&& [_it, emplaced] = visited.try_emplace(id, colour::grey);
                if (!emplaced) {
                    // Already visited.
                    return;
                }
                stack.push_back(id);

                for (vertex_id out: v.outs) {
                    if (visited.count(out)) {
                        // We have seen this vertex before. Is there a
                        // cycle anywhere?
                        if (auto it = std::find(stack.begin(), stack.end(), out); it != stack.end()) {
                            // Yes, [it, end()] is a cycle.
                            std::vector<T> cycle;
                            for (auto it2 = it; it2 != stack.end(); it2++) {
                                auto it3 = _vertices.find(*it2);
                                assert(it3 != _vertices.end());
                                cycle.push_back(*(it3->second.value));
                            }
                            throw not_a_dag(std::move(cycle));
                        }
                    }
                    else {
                        auto it = _vertices.find(out);
                        assert(it != _vertices.end());
                        self(self, out, it->second);
                    }
                }

                if (visited[id] == colour::grey) {
                    tsorted.push_back(std::cref(*(v.value)));
                    visited[id] = colour::black;
                }
                stack.resize(depth);
            };

        for (auto const& [id, v]: _vertices) {
            go(go, id, v);
            assert(stack.empty());
        }

        return tsorted;
    }
}

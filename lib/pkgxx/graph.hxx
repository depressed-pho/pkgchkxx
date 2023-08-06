#pragma once

#include <deque>
#include <exception>
#include <functional>
#include <cassert>
#include <map>
#include <set>
#include <optional>
#include <sstream>
#include <type_traits>
#include <utility>
#include <vector>

#include <pkgxx/unwrap.hxx>

namespace pkgxx {
    /// A cycle has been detected while tsorting a graph.
    template <typename VertexT, typename EdgeT = void>
    struct not_a_dag: virtual std::runtime_error {
#if !defined(DOXYGEN)
        not_a_dag(std::vector<VertexT>&& vertices_, std::vector<EdgeT>&& edges_)
            : std::runtime_error("")
            , _vertices(std::move(vertices_))
            , _edges(std::move(edges_)) {}

        virtual char const*
        what() const noexcept override;
#endif

        /// Return a string representing the cycle.
        std::string
        cycle() const;

    private:
        std::vector<VertexT> _vertices;
        std::vector<EdgeT> _edges;
        mutable std::optional<std::string> _msg;
    };

    /// A special case when EdgeT is void.
    template <typename VertexT>
    struct not_a_dag<VertexT, void>: virtual std::runtime_error {
#if !defined(DOXYGEN)
        not_a_dag(std::vector<VertexT>&& vertices_)
            : std::runtime_error("")
            , _vertices(std::move(vertices_)) {}

        virtual char const*
        what() const noexcept override;
#endif

        /// Return a string representing the cycle.
        std::string
        cycle() const;

    private:
        std::vector<VertexT> _vertices;
        mutable std::optional<std::string> _msg;
    };

    /** A directed graph that is barely enough for topological sorting. The
     * type \c VertexT is the type of vertices and need to be
     * copy-constructible, totally ordered, and outputtable. \c EdgeT is
     * the type of edges and need to be either \c void or some
     * copy-constructible, assignable, and outputtable type. Instances with
     * \c IsBidirectional being \c true have slightly more overhead but
     * supports additional operations.
     */
    template <typename VertexT,
              typename EdgeT = void,
              bool IsBidirectional = false>
    struct graph {
        /** The type of <tt>std::reference_wrapper<VertexT const></tt>,
         * works even when VertexT is itself a reference wrapper.
         */
        using vertex_reference_type =
            std::reference_wrapper<
                std::decay_t<
                    unwrap_ref_decay_t<VertexT>
                    > const
            >;

        /** The type of <tt>std::reference_wrapper<EdgeT const></tt>, works
         * even when EdgeT is itself a reference wrapper. Only makes sense
         * if \c EdgeT is not \c void.
         */
        using edge_reference_type =
            std::reference_wrapper<
                std::decay_t<
                    unwrap_ref_decay_t<EdgeT>
                    > const
            >;

        /** Add a vertex to the graph if it doesn't already exist. */
        void
        add_vertex(VertexT const& value) {
            add_vertex_impl(value);
        }

        /** Remove a vertex from the graph if it exists. Only available for
         * bidirectional graphs. */
        template <bool IsBidi = IsBidirectional>
        std::enable_if_t<IsBidi, void>
        remove_vertex(VertexT const& value);

        /** Add an edge between two vertices to the graph. This method only
         * exists for graphs whose \c EdgeT type is \c void. The vertices
         * don't need to be added prior to calling this method.
         */
        template <typename EdgeT_ = EdgeT>
        std::enable_if_t<std::is_same_v<EdgeT_, void>, void>
        add_edge(VertexT const& src, VertexT const& dest);

        /** Add an edge between two vertices to the graph. If an edge
         * already exists, then it will be overwritten with a new one. This
         * method only exists for graphs whose \c EdgeT type is not \c
         * void. The vertices don't need to be added prior to calling this
         * method.
         */
        template <typename EdgeT_ = EdgeT>
        std::enable_if_t<!std::is_same_v<EdgeT_, void>, void>
        add_edge(VertexT const& src, VertexT const& dest, EdgeT_ const& edge);

        /** Remove an edge from the graph if it exists. */
        void
        remove_edge(VertexT const& src, VertexT const& dest);

        /** Remove in-edges to a vertex if it exists. Only available for
         * bidirectional graphs. */
        template <bool IsBidi = IsBidirectional>
        std::enable_if_t<IsBidi, void>
        remove_in_edges(VertexT const& src);

        /** Remove out-edges from a vertex if it exists. */
        void
        remove_out_edges(VertexT const& src);

        /** See if the graph has the given vertex. */
        bool
        has_vertex(VertexT const& value) const {
            return _vertex_id_of.count(value) != 0;
        }

        /** Return the set of out-edges from a vertex, or \c std::nullopt
         * if no such vertex exists. The set will be invalidated when the
         * vertex or edges are removed. */
        std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::set<vertex_reference_type, std::less<VertexT>>,
                std::map<vertex_reference_type, edge_reference_type, std::less<VertexT>>
                >
            >
        out_edges(VertexT const& value) const;

        /** Return the set of in-edges to a vertex, or \c std::nullopt if
         * no such vertex exists. The set will be invalidated when the
         * vertex or edges are removed. Only available for bidirectional
         * graphs. */
        template <bool IsBidi = IsBidirectional>
        std::enable_if_t<
            IsBidi,
            std::optional<
                std::conditional_t<
                    std::is_same_v<EdgeT, void>,
                    std::set<vertex_reference_type, std::less<VertexT>>,
                    std::map<vertex_reference_type, edge_reference_type, std::less<VertexT>>
                    >
                >
            >
        in_edges(VertexT const& value) const;

        /** Compute the shortest path between two vertices, if such a path
         * exists. Edges are assumed to have the same weight. */
        std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::deque<vertex_reference_type>,
                std::pair<
                    std::deque<vertex_reference_type>,
                    std::deque<edge_reference_type>
                    >
                >
            >
        shortest_path(VertexT const& src, VertexT const& dest) const;

        /** Perform a topological sort on the graph. Vertices that have no
         * out-edges will appear first. Edges are assumed to have the same
         * weight. If it has a cycle \ref not_a_dag will be thrown. Passing
         * \c true as \c cache will cause the result to be cached so that
         * it won't be re-tsorted until the graph is modified in any way.
         */
        std::vector<vertex_reference_type>
        tsort(bool cache = true) const;

    private:
        using vertex_id = unsigned long;

        struct empty {};
        struct bidi_vertex {
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::set<vertex_id>,
                std::map<vertex_id, edge_reference_type>
                > ins;
        };
        struct vertex: public std::conditional_t<IsBidirectional, bidi_vertex, empty> {
            vertex(VertexT const& value_)
                : value(&value_) {}

            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::set<vertex_id>,
                std::map<vertex_id, EdgeT>
                > outs;
            VertexT const* value;
        };

        enum class colour {
            white, // unvisited vertices
            grey,  // visited but has unvisited edges
            black  // visited and has no unvisited edges
        };

        vertex_id
        add_vertex_impl(VertexT const& value);

        std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::deque<vertex_reference_type>,
                std::pair<
                    std::deque<vertex_reference_type>,
                    std::deque<edge_reference_type>
                    >
                >
            >
        shortest_path_impl(vertex_id const& src, vertex_id const& dest) const;

        std::map<VertexT, vertex_id> _vertex_id_of;
        std::map<vertex_id, vertex> _vertices;

        std::optional<std::vector<vertex_reference_type>> mutable _tsort_cache;
    };

    // Implementation

#if !defined(DOXYGEN)
    template <typename VertexT, typename EdgeT>
    char const*
    not_a_dag<VertexT, EdgeT>::what() const noexcept {
        if (!_msg) {
            _msg = "found a cycle: " + cycle();
        }
        return _msg->c_str();
    }

    template <typename VertexT, typename EdgeT>
    std::string
    not_a_dag<VertexT, EdgeT>::cycle() const {
        std::stringstream ss;
        for (auto value = _vertices.begin(), edge = _edges.begin();
             value != _vertices.end();
             value++) {

            ss << *value;
            if (edge != _edges.end()) {
                ss << " -[" << *edge << "]-> ";
                edge++;
            }
        }
        return ss.str();
    }

    template <typename VertexT>
    char const*
    not_a_dag<VertexT, void>::what() const noexcept {
        if (!_msg) {
            _msg = "found a cycle: " + cycle();
        }
        return _msg->c_str();
    }

    template <typename VertexT>
    std::string
    not_a_dag<VertexT, void>::cycle() const {
        std::stringstream ss;
        bool is_first = true;

        for (auto const& value: _vertices) {
            if (is_first) {
                is_first = false;
            }
            else {
                ss << " -> ";
            }
            ss << value;
        }
        return ss.str();
    }
#endif

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    typename graph<VertexT, EdgeT, IsBidirectional>::vertex_id
    graph<VertexT, EdgeT, IsBidirectional>::add_vertex_impl(VertexT const& value) {
        auto&& [it, emplaced] = _vertex_id_of.try_emplace(value, _vertex_id_of.size());
        if (emplaced) {
            _vertices.try_emplace(it->second, it->first);
            _tsort_cache.reset();
        }
        return it->second;
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    template <bool IsBidi>
    std::enable_if_t<IsBidi, void>
    graph<VertexT, EdgeT, IsBidirectional>::remove_vertex(VertexT const& value) {
        static_assert(IsBidi == IsBidirectional, "can't explicitly specialise");

        if (auto id = _vertex_id_of.find(value); id != _vertex_id_of.end()) {
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
            _tsort_cache.reset();
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    template <typename EdgeT_>
    std::enable_if_t<std::is_same_v<EdgeT_, void>, void>
    graph<VertexT, EdgeT, IsBidirectional>::add_edge(VertexT const& src, VertexT const& dest) {
        static_assert(std::is_same_v<EdgeT_, EdgeT>, "can't explicitly specialise");

        vertex_id const src_id  = add_vertex_impl(src);
        vertex_id const dest_id = add_vertex_impl(dest);

        auto sv = _vertices.find(src_id);
        assert(sv != _vertices.end());
        if (auto&& [_out, inserted] = sv->second.outs.insert(dest_id); inserted) {
            _tsort_cache.reset();
        }

        if constexpr (IsBidirectional) {
            auto dv = _vertices.find(dest_id);
            assert(dv != _vertices.end());
            dv->second.ins.insert(src_id);
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    template <typename EdgeT_>
    std::enable_if_t<!std::is_same_v<EdgeT_, void>, void>
    graph<VertexT, EdgeT, IsBidirectional>::add_edge(VertexT const& src, VertexT const& dest, EdgeT_ const& edge) {
        static_assert(std::is_same_v<EdgeT_, EdgeT>, "can't explicitly specialise");

        vertex_id const src_id  = add_vertex_impl(src);
        vertex_id const dest_id = add_vertex_impl(dest);

        auto sv = _vertices.find(src_id);
        assert(sv != _vertices.end());
        auto [out, _inserted] = sv->second.outs.insert_or_assign(dest_id, edge);
        _tsort_cache.reset();

        if constexpr (IsBidirectional) {
            auto dv = _vertices.find(dest_id);
            assert(dv != _vertices.end());
            dv->second.ins.insert_or_assign(src_id, std::cref(out->second));
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    void
    graph<VertexT, EdgeT, IsBidirectional>::remove_edge(VertexT const& src, VertexT const& dest) {
        if (auto src_id = _vertex_id_of.find(src); src_id != _vertex_id_of.end()) {
            auto src_v = _vertices.find(src_id->second);
            assert(src_v != _vertices.end());

            if (auto dest_id = _vertex_id_of.find(dest); dest_id != _vertex_id_of.end()) {
                if (src_v->second.outs.erase(dest_id->second)) {
                    if constexpr (IsBidirectional) {
                        auto dest_v = _vertices.find(dest_id->second);
                        assert(dest_v != _vertices.end());

                        dest_v->second.ins.erase(src_id->second);
                    }
                    _tsort_cache.reset();
                }
            }
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    template <bool IsBidi>
    std::enable_if_t<IsBidi, void>
    graph<VertexT, EdgeT, IsBidirectional>::remove_in_edges(VertexT const& value) {
        static_assert(IsBidi == IsBidirectional, "can't explicitly specialise");

        if (auto dest_id = _vertex_id_of.find(value); dest_id != _vertex_id_of.end()) {
            auto dest_v = _vertices.find(dest_id->second);
            assert(dest_v != _vertices.end());

            if (!dest_v->second.ins.empty()) {
                for (auto src: dest_v->second.ins) {
                    vertex_id src_id;
                    if constexpr (std::is_same_v<EdgeT, void>) {
                        src_id = src;
                    }
                    else {
                        src_id = src.first;
                    }

                    auto src_v = _vertices.find(src_id);
                    assert(src_v != _vertices.end());

                    src_v->second.outs.erase(dest_id->second);
                }
                dest_v->second.ins.clear();
                _tsort_cache.reset();
            }
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    void
    graph<VertexT, EdgeT, IsBidirectional>::remove_out_edges(VertexT const& value) {
        if (auto src_id = _vertex_id_of.find(value); src_id != _vertex_id_of.end()) {
            auto src_v = _vertices.find(src_id->second);
            assert(src_v != _vertices.end());

            if (!src_v->second.outs.empty()) {
                if constexpr (IsBidirectional) {
                    for (auto dest: src_v->second.outs) {
                        vertex_id dest_id;
                        if constexpr (std::is_same_v<EdgeT, void>) {
                            dest_id = dest;
                        }
                        else {
                            dest_id = dest.first;
                        }

                        auto dest_v = _vertices.find(dest_id);
                        assert(dest_v != _vertices.end());

                        dest_v->second.ins.erase(src_id->second);
                    }
                }
                src_v->second.outs.clear();
                _tsort_cache.reset();
            }
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    std::optional<
        std::conditional_t<
            std::is_same_v<EdgeT, void>,
            std::set<
                typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type,
                std::less<VertexT>
                >,
            std::map<
                typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type,
                typename graph<VertexT, EdgeT, IsBidirectional>::edge_reference_type,
                std::less<VertexT>
                >
            >
        >
    graph<VertexT, EdgeT, IsBidirectional>::out_edges(VertexT const& value) const {
        if (auto id = _vertex_id_of.find(value); id != _vertex_id_of.end()) {
            auto v = _vertices.find(id->second);
            assert(v != _vertices.end());

            typename decltype(out_edges(value))::value_type ret;
            if constexpr (std::is_same_v<EdgeT, void>) {
                for (auto out_id: v->second.outs) {
                    auto out_v = _vertices.find(out_id);
                    assert(out_v != _vertices.end());

                    ret.insert(std::cref(*(out_v->second.value)));
                }
            }
            else {
                for (auto const& [out_id, out_edge]: v->second.outs) {
                    auto out_v = _vertices.find(out_id);
                    assert(out_v != _vertices.end());

                    ret.emplace(std::cref(*(out_v->second.value)), std::cref(out_edge));
                }
            }
            return ret;
        }
        else {
            return std::nullopt;
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    template <bool IsBidi>
    std::enable_if_t<
        IsBidi,
        std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::set<
                    typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type,
                    std::less<VertexT>
                    >,
                std::map<
                    typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type,
                    typename graph<VertexT, EdgeT, IsBidirectional>::edge_reference_type,
                    std::less<VertexT>
                    >
                >
            >
        >
    graph<VertexT, EdgeT, IsBidirectional>::in_edges(VertexT const& value) const {
        static_assert(IsBidi == IsBidirectional, "can't explicitly specialise");

        if (auto id = _vertex_id_of.find(value); id != _vertex_id_of.end()) {
            auto v = _vertices.find(id->second);
            assert(v != _vertices.end());

            typename decltype(in_edges(value))::value_type ret;
            if constexpr (std::is_same_v<EdgeT, void>) {
                for (auto in_id: v->second.ins) {
                    auto in_v = _vertices.find(in_id);
                    assert(in_v != _vertices.end());

                    ret.insert(std::cref(*(in_v->second.value)));
                }
            }
            else {
                for (auto const& [in_id, in_edge]: v->second.ins) {
                    auto in_v = _vertices.find(in_id);
                    assert(in_v != _vertices.end());

                    ret.emplace(std::cref(*(in_v->second.value)), in_edge);
                }
            }
            return ret;
        }
        else {
            return std::nullopt;
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    std::vector<
        typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type
        >
    graph<VertexT, EdgeT, IsBidirectional>::tsort(bool cache) const {
        if (cache && _tsort_cache) {
            return *_tsort_cache;
        }

        std::map<vertex_id, colour> visited;
        std::vector<vertex_reference_type> tsorted;

        for (auto const& [id, _v]: _vertices) {
            visited[id] = colour::white;
        }

        auto const go =
            [&](auto const& self, vertex_id id, vertex const& v) -> void {
                auto c = visited.find(id);
                assert(c != visited.end());

                if (c->second == colour::white) {
                    c->second = colour::grey;
                }
                else {
                    return; // Already visited.
                }

                for (auto const& out: v.outs) {
                    vertex_id out_id;
                    if constexpr (std::is_same_v<EdgeT, void>) {
                        out_id = out;
                    }
                    else {
                        out_id = out.first;
                    }
                    auto out_c = visited.find(out_id);
                    assert(out_c != visited.end());

                    switch (out_c->second) {
                    case colour::white:
                        {
                            auto out_v = _vertices.find(out_id);
                            assert(out_v != _vertices.end());
                            // This is a recursive DFS. Maybe we should
                            // turn this into a non-recursive one?
                            self(self, out_id, out_v->second);
                        }
                        break;

                    case colour::grey:
                        // The edge "id" -> "out_id" forms a cycle, which
                        // means there must be a path going from "out_id"
                        // all the way back to "id". Find it using BFS and
                        // raise an exception.
                        if constexpr (std::is_same_v<EdgeT, void>) {
                            std::vector<VertexT> vertices;
                            auto const path = shortest_path_impl(out_id, id).value();
                            for (auto const& value: path) {
                                vertices.push_back(value);
                            }
                            auto out_v = _vertices.find(out_id);
                            assert(out_v != _vertices.end());
                            vertices.push_back(*(out_v->second.value));
                            throw not_a_dag<VertexT, void>(std::move(vertices));
                        }
                        else {
                            std::vector<VertexT> vertices;
                            std::vector<EdgeT> edges;
                            auto const path = shortest_path_impl(out_id, id).value();
                            for (auto const& value: path.first) {
                                vertices.push_back(value);
                            }
                            for (auto const& edge: path.second) {
                                edges.push_back(edge);
                            }
                            if (id != out_id) {
                                auto out_v = _vertices.find(out_id);
                                assert(out_v != _vertices.end());
                                vertices.push_back(*(out_v->second.value));
                                edges.push_back(out.second);
                            }
                            throw not_a_dag<VertexT, EdgeT>(std::move(vertices), std::move(edges));
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

        if (cache) {
            _tsort_cache = tsorted;
        }
        return tsorted;
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type>,
                std::pair<
                    std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type>,
                    std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::edge_reference_type>
                    >
                >
            >
    graph<VertexT, EdgeT, IsBidirectional>::shortest_path(VertexT const& src, VertexT const& dest) const {
        auto src_id  = _vertex_id_of.find(src);
        auto dest_id = _vertex_id_of.find(dest);

        if (src_id != _vertex_id_of.end() && dest_id != _vertex_id_of.end()) {
            return shortest_path_impl(src_id->second, dest_id->second);
        }
        else {
            return std::nullopt;
        }
    }

    template <typename VertexT, typename EdgeT, bool IsBidirectional>
    std::optional<
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type>,
                std::pair<
                    std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::vertex_reference_type>,
                    std::deque<typename graph<VertexT, EdgeT, IsBidirectional>::edge_reference_type>
                    >
                >
            >
    graph<VertexT, EdgeT, IsBidirectional>::shortest_path_impl(vertex_id const& src, vertex_id const& dest) const {
        std::map<vertex_id, colour> visited;
        std::map<
            vertex_id,
            std::conditional_t<
                std::is_same_v<EdgeT, void>,
                vertex_id,
                std::pair<
                    vertex_id,
                    typename graph<VertexT, EdgeT, IsBidirectional>::edge_reference_type
                    >
                >
            > predecessor_of;
        std::deque<vertex_id> queue;

        for (auto const& [id, _v]: _vertices) {
            visited[id] = (id == src) ? colour::grey : colour::white;
        }

        auto const& go =
            [&](vertex_id id, vertex const& v) -> bool {
                for (auto const& out: v.outs) {
                    vertex_id out_id;
                    if constexpr (std::is_same_v<EdgeT, void>) {
                        out_id = out;
                    }
                    else {
                        out_id = out.first;
                    }

                    auto c = visited.find(out_id);
                    assert(c != visited.end());
                    if (c->second == colour::white) {
                        c->second = colour::grey;
                        if constexpr (std::is_same_v<EdgeT, void>) {
                            predecessor_of[out_id] = id;
                        }
                        else {
                            predecessor_of[out_id] = std::make_pair(id, out.second);
                        }
                        queue.push_back(out_id);
                    }

                    if (out_id == dest) {
                        // Found the final destination.
                        return true;
                    }
                }
                return false;
            };

        queue.push_back(src);

        while (!queue.empty()) {
            vertex_id id = queue.front();
            queue.pop_front();

            auto it = _vertices.find(id);
            assert(it != _vertices.end());

            if (go(id, it->second)) {
                // We found the final destination and we have a predecessor
                // recorded for each vertex we have visited. Reconstruct a
                // path starting from "src" to "dest" by visiting
                // predecessors of "dest" in the reverse order.
                typename decltype(shortest_path_impl(src, dest))::value_type path;
                vertex_id id = dest;
                while (true) {
                    auto pred = predecessor_of.find(id);
                    if (pred == predecessor_of.end()) {
                        // There is no predecessor for "src".
                        assert(id == src);
                        break;
                    }

                    auto v = _vertices.find(id);
                    assert(v != _vertices.end());

                    if constexpr (std::is_same_v<EdgeT, void>) {
                        path.push_front(std::cref(*(v->second.value)));
                        id = pred->second;
                    }
                    else {
                        path.first.push_front(std::cref(*(v->second.value)));
                        path.second.push_front(pred->second.second);
                        id = pred->second.first;
                    }
                }
                auto src_v = _vertices.find(src);
                assert(src_v != _vertices.end());
                if constexpr (std::is_same_v<EdgeT, void>) {
                    path.push_front(std::cref(*(src_v->second.value)));
                }
                else {
                    path.first.push_front(std::cref(*(src_v->second.value)));
                    assert(path.second.size() == path.first.size() + 1);
                }
                return path;
            }
        }

        return std::nullopt;
    }
}

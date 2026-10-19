#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include <arm_neon.h>

struct Graph {
    std::vector<uint32_t> offsets;
    std::vector<uint32_t> edges;
};

static Graph make_graph(uint32_t n, uint32_t m, uint64_t seed) {
    if (n < 2) throw std::runtime_error("n must be >= 2");
    const uint64_t max_edges = uint64_t(n) * (n - 1) / 2;
    if (m > max_edges) throw std::runtime_error("m exceeds maximum simple undirected graph edges");

    std::mt19937_64 rng(seed);
    std::set<std::pair<uint32_t, uint32_t>> unique_edges;
    while (unique_edges.size() < m) {
        uint32_t u = uint32_t(rng() % n);
        uint32_t v = uint32_t(rng() % n);
        if (u == v) continue;
        if (u > v) std::swap(u, v);
        unique_edges.emplace(u, v);
    }
    std::vector<std::pair<uint32_t, uint32_t>> es(unique_edges.begin(), unique_edges.end());
    std::vector<uint64_t> degree(n, 0);
    for (auto [u, v] : es) { ++degree[u]; ++degree[v]; }
    Graph g;
    g.offsets.resize(size_t(n) + 1, 0);
    for (uint32_t i = 0; i < n; ++i)
        g.offsets[i + 1] = g.offsets[i] + uint32_t(degree[i]);
    g.edges.resize(size_t(g.offsets[n]));
    std::vector<uint32_t> cursor = g.offsets;
    for (auto [u, v] : es) {
        g.edges[cursor[u]++] = v;
        g.edges[cursor[v]++] = u;
    }
    return g;
}

static std::vector<int32_t> bfs_scalar(const Graph& g, uint32_t source) {
    const uint32_t n = uint32_t(g.offsets.size() - 1);
    std::vector<int32_t> dist(n, -1);
    std::vector<uint32_t> q(n);
    uint32_t head = 0, tail = 0;
    dist[source] = 0;
    q[tail++] = source;
    while (head < tail) {
        const uint32_t u = q[head++];
        const int32_t next_dist = dist[u] + 1;
        for (uint32_t e = g.offsets[u]; e < g.offsets[u + 1]; ++e) {
            const uint32_t v = g.edges[e];
            if (dist[v] == -1) {
                dist[v] = next_dist;
                q[tail++] = v;
            }
        }
    }
    return dist;
}

static std::vector<int32_t> bfs_reference(const Graph& g, uint32_t source) {
    const uint32_t n = uint32_t(g.offsets.size() - 1);
    std::vector<int32_t> dist(n, -1);
    std::queue<uint32_t> q;
    dist[source] = 0;
    q.push(source);
    while (!q.empty()) {
        uint32_t u = q.front(); q.pop();
        for (uint32_t e = g.offsets[u]; e < g.offsets[u + 1]; ++e) {
            uint32_t v = g.edges[e];
            if (dist[v] == -1) {
                dist[v] = dist[u] + 1;
                q.push(v);
            }
        }
    }
    return dist;
}

static std::vector<int32_t> bfs_neon_scan(const Graph& g, uint32_t source) {
    const uint32_t n = uint32_t(g.offsets.size() - 1);
    std::vector<int32_t> dist(n, -1);
    std::vector<uint32_t> q(n);
    uint32_t head = 0, tail = 0;
    dist[source] = 0;
    q[tail++] = source;
    while (head < tail) {
        const uint32_t u = q[head++];
        const int32_t next_dist = dist[u] + 1;
        uint32_t e = g.offsets[u];
        const uint32_t end = g.offsets[u + 1];
        for (; e + 4 <= end; e += 4) {
            uint32x4_t ids = vld1q_u32(&g.edges[e]);
            alignas(16) uint32_t lane[4];
            vst1q_u32(lane, ids);
            for (int k = 0; k < 4; ++k) {
                const uint32_t v = lane[k];
                if (dist[v] == -1) {
                    dist[v] = next_dist;
                    q[tail++] = v;
                }
            }
        }
        for (; e < end; ++e) {
            const uint32_t v = g.edges[e];
            if (dist[v] == -1) {
                dist[v] = next_dist;
                q[tail++] = v;
            }
        }
    }
    return dist;
}

static double median(std::vector<double> x) {
    std::sort(x.begin(), x.end());
    const size_t n = x.size();
    return n % 2 ? x[n / 2] : (x[n / 2 - 1] + x[n / 2]) / 2.0;
}

template<class Fn>
static double measure_ms(Fn&& fn, int repeats, std::vector<int32_t>& output) {
    std::vector<double> times;
    times.reserve(repeats);
    for (int i = 0; i < repeats; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        output = fn();
        const auto t1 = std::chrono::steady_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    return median(times);
}

int main(int argc, char** argv) {
    try {
        const uint32_t n = argc > 1 ? uint32_t(std::stoul(argv[1])) : 100000;
        const uint32_t m = argc > 2 ? uint32_t(std::stoul(argv[2])) : 500000;
        const uint64_t seed = argc > 3 ? std::stoull(argv[3]) : 12345;
        const int repeats = argc > 4 ? std::stoi(argv[4]) : 9;
        if (repeats < 1) throw std::runtime_error("repeats must be >= 1");

        const Graph g = make_graph(n, m, seed);
        const uint32_t source = 0;
        const auto reference = bfs_reference(g, source);
        std::vector<int32_t> result;
        const double ms = measure_ms([&] { return bfs_scalar(g, source); }, repeats, result);
        if (result != reference) {
            std::cerr << "ERROR: BFS result differs from reference\n";
            return 2;
        }
        const size_t reached = size_t(std::count_if(result.begin(), result.end(),
                                                    [](int32_t d) { return d >= 0; }));
        std::cout << "variant,vertices,undirected_edges,seed,repeats,median_ms,reachable,checksum\n";
        uint64_t checksum = 0;
        for (uint32_t i = 0; i < n; ++i)
            checksum += uint64_t(uint32_t(result[i])) * (uint64_t(i) + 1);
        std::cout << "scalar," << n << ',' << m << ',' << seed << ',' << repeats << ','
                  << std::fixed << std::setprecision(3) << ms << ',' << reached << ','
                  << checksum << '\n';

        std::vector<int32_t> neon_result;
        const double neon_ms = measure_ms([&] { return bfs_neon_scan(g, source); }, repeats, neon_result);
        if (neon_result != reference) {
            std::cerr << "ERROR: NEON result differs from reference\n";
            return 3;
        }
        std::cout << "neon_scan," << n << ',' << m << ',' << seed << ',' << repeats << ','
                  << std::fixed << std::setprecision(3) << neon_ms << ',' << reached << ','
                  << checksum << '\n';

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}

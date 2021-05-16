//
// Created by pupa on 2021/5/14.
//

#define JC_VORONOI_IMPLEMENTATION

#include "JCVoronoi.h"

#include <set>
#include <unordered_map>


jcv_rect bounding_rect{0, 0, -1, -1};


void delaunay_triangulation(MatrixX2rd& points_, MatrixX2rd& V, Eigen::MatrixX3i& F) {
    jcv_diagram diagram_{nullptr};
    jcv_diagram_generate(points_.rows(), (jcv_point*)points_.data(), &bounding_rect, 0, &diagram_);

    using TriLet = EigenLet<int, 3> ;
    std::set<TriLet> TMap;
    const jcv_site* sites = jcv_diagram_get_sites(&diagram_);
//    for (int i = 0; i < diagram_.numsites; ++i)
//        V_map[ sites[i].p ] = sites[i].index;

    for (int i = 0; i < diagram_.numsites; ++i) {
        for(auto graph_edge = sites[i].edges; graph_edge; graph_edge = graph_edge->next) {
            auto next_edge = graph_edge->next ? graph_edge->next: sites[i].edges;
            jcv_site* curr_neighbor = graph_edge->neighbor;
            jcv_site* next_neighbor = next_edge->neighbor;
//            if(V_map.find(graph_edge->pos[0]) == V_map.end())
//                V_map[graph_edge->pos[0]] = V_map.size();
//            if(V_map.find(graph_edge->pos[1]) == V_map.end())
//                V_map[graph_edge->pos[1]] = V_map.size();
            if(curr_neighbor && next_neighbor) {
                TriLet triangle(curr_neighbor->index, sites[i].index,  next_neighbor->index);
                triangle.rotate();
                if(TMap.find(triangle) == TMap.end())
                    TMap.insert(triangle);
//                triangles.push_back({sites[i].p, curr_neighbor->p, next_neighbor->p});
            }
//            else if (curr_neighbor && next_neighbor == nullptr) {
//                triangles.push_back({sites[i].p, curr_neighbor->p, graph_edge->pos[1]});
//            }else if (curr_neighbor == nullptr && next_neighbor) {
//                triangles.push_back({sites[i].p, graph_edge->pos[0], graph_edge->pos[1]});
//                triangles.push_back({sites[i].p, graph_edge->pos[1], next_neighbor->p});
//            }else {
//                triangles.push_back({sites[i].p, graph_edge->pos[0], graph_edge->pos[1]});
//            }
        }
    }

    V.resize(diagram_.numsites, 2);
    F.resize(TMap.size(), 3);
    for (int i = 0; i < diagram_.numsites; ++i)
        V.row(sites[i].index) = Eigen::RowVector2d(sites[i].p.x, sites[i].p.y);

    size_t idx = 0;
    for (auto it = TMap.begin(); it != TMap.end(); it++)
        F.row(idx++) = *it;

}

Eigen::MatrixX3i tri_tri_adjacency(const Eigen::MatrixX3i& F) {
    std::unordered_map< EigenLet<int, 2>, Eigen::Vector2i, EigenLetHash<EigenLet<int, 2>> > edge_mark;
    Eigen::MatrixX3i TT(F.rows(), 3);
    TT.setConstant(-1);
    for(size_t fi= 0; fi < F.rows(); fi++) {
        for(int i = 0; i < 3; i++) {
            EigenLet<int, 2> e(F(fi, i), F(fi, (i+1)%3));
            e.rotate();
            if(edge_mark.find(e) != edge_mark.end()) {
                auto v_pair = edge_mark[e];
                TT(v_pair[0], v_pair[1]) = fi;
                TT(fi, i) = v_pair[0];
            }else
                edge_mark[e] = Eigen::Vector2i(fi, i);
        }
    }
    return TT;
}

inline Point2d vector (const jcv_point& p) { return Point2d(p.x, p.y); }


Point2d weighted_cell_centroid(const ImageSampler& density, const jcv_site* site)  {
    Point2d pc = vector( site->p );
    Eigen::RowVector4d sum(0, 0, 0, 0), t;
    for(auto graph_edge = site->edges; graph_edge; graph_edge = graph_edge->next) {
        Point2d p1 = vector( graph_edge->pos[0] );
        Point2d p2 = vector( graph_edge->pos[1] );
        t = density.weight_moment(p1, p2, pc);
        double area_ = area(p1, p2, pc);
        sum.leftCols(2) += t.leftCols(2)/t.z() * t.z()/t.w() * area_;
        sum.w() += t.z()/t.w() * area_;
    }
//    std::cout << site->p.x <<  ',' << site->p.y << " >>> " << sum.leftCols(2) << std::endl;
    return sum.leftCols(2)/sum.w();
}

double cvt_lloyd_relaxation(MatrixX2rd& points_, const ImageSampler& density) {
    bounding_rect.max = jcv_point{(double)density.cols(), (double)density.rows() };
    jcv_diagram diagram_{nullptr};
    auto rect = density.bound_rect();
    jcv_diagram_generate(points_.rows(), (jcv_point*)points_.data(), (jcv_rect*)(rect.data()), 0, &diagram_);
    const jcv_site* sites = jcv_diagram_get_sites(&diagram_);
    auto prev = points_;
    for (int i = 0; i < diagram_.numsites; ++i)
        points_.row(sites[i].index) = weighted_cell_centroid(density, &sites[i]);
    if(diagram_.internal) jcv_diagram_free(&diagram_);
    return (prev-points_).cwiseAbs().maxCoeff();
}


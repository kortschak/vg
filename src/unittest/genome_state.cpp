//
//  genome_state.cpp
//
//  Unit tests for SnarlState, GenomeState and related functions
//

#include <stdio.h>
#include <iostream>
#include <set>
#include "catch.hpp"
#include "../json2pb.h"
#include "../genotypekit.hpp"
#include "../genome_state.hpp"


namespace vg {
namespace unittest {

using namespace std;

TEST_CASE("SnarlState can hold haplotypes", "[snarlstate][genomestate]") {
    

    // This graph will have a snarl from 1 to 8, a snarl from 2 to 7,
    // and a snarl from 3 to 5, all nested in each other.
    VG graph;
        
    Node* n1 = graph.create_node("GCA");
    Node* n2 = graph.create_node("T");
    Node* n3 = graph.create_node("G");
    Node* n4 = graph.create_node("CTGA");
    Node* n5 = graph.create_node("GCA");
    Node* n6 = graph.create_node("T");
    Node* n7 = graph.create_node("G");
    Node* n8 = graph.create_node("CTGA");
    
    Edge* e1 = graph.create_edge(n1, n2);
    Edge* e2 = graph.create_edge(n1, n8);
    Edge* e3 = graph.create_edge(n2, n3);
    Edge* e4 = graph.create_edge(n2, n6);
    Edge* e5 = graph.create_edge(n3, n4);
    Edge* e6 = graph.create_edge(n3, n5);
    Edge* e7 = graph.create_edge(n4, n5);
    Edge* e8 = graph.create_edge(n5, n7);
    Edge* e9 = graph.create_edge(n6, n7);
    Edge* e10 = graph.create_edge(n7, n8);
    
    // Work out its snarls
    CactusSnarlFinder bubble_finder(graph);
    SnarlManager snarl_manager = bubble_finder.find_snarls();
    
    // Get the top snarl
    const Snarl* top_snarl = snarl_manager.top_level_snarls().at(0);
    
    // Make sure it's what we expect.
    REQUIRE(top_snarl->start().node_id() == 1);
    REQUIRE(top_snarl->end().node_id() == 8);
    
    // And get its net graph
    NetGraph net_graph = snarl_manager.net_graph_of(top_snarl, &graph, true);
    
    // Make a SnarlState for it
    SnarlState state(&net_graph);
    
    SECTION("The state starts empty") {
        REQUIRE(state.size() == 0);
    }
    
    SECTION("A haplotype with lane numbers can be added in lane 0 when the SnarlState is empty") {
        // Make a haplotype
        vector<pair<handle_t, size_t>> annotated_haplotype;
        
        // Say we go 1, 2 (which is a child snarl), 8
        annotated_haplotype.emplace_back(net_graph.get_handle(1, false), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(2, false), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(8, false), 0);
        
        // Put it in the state
        state.insert(annotated_haplotype);
        
        SECTION("The state now has 1 haplotype") {
            REQUIRE(state.size() == 1);
        }
        
        SECTION("The haplotype can be traced back again") {
            vector<pair<handle_t, size_t>> recovered;
            
            state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                recovered.emplace_back(visit, local_lane);
            });
            
            REQUIRE(recovered == annotated_haplotype);
        }
        
        SECTION("The haplotype can be traced in reverse") {
            vector<pair<handle_t, size_t>> recovered;
            
            state.trace(0, true, [&](const handle_t& visit, size_t local_lane) {
                recovered.emplace_back(net_graph.flip(visit), local_lane);
            });
            
            reverse(recovered.begin(), recovered.end());
            
            REQUIRE(recovered == annotated_haplotype);
        }
        
        SECTION("A haplotype with lane numbers can be inserted before an existing haplotype") {
            
            vector<pair<handle_t, size_t>> hap2;
            // Say we go 1, 8 directly in lane 0
            hap2.emplace_back(net_graph.get_handle(1, false), 0);
            hap2.emplace_back(net_graph.get_handle(8, false), 0);
            
            // Put it in the state
            state.insert(hap2);
            
            SECTION("The state now has 2 haplotypes") {
                REQUIRE(state.size() == 2);
            }
            
            SECTION("The new haplotype can be traced back again") {
                vector<pair<handle_t, size_t>> recovered;
                
                state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                    recovered.emplace_back(visit, local_lane);
                });
                
                REQUIRE(recovered == hap2);
            }
            
            SECTION("The old haplotype can be traced back again") {
                vector<pair<handle_t, size_t>> recovered;
                
                state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                    recovered.emplace_back(visit, local_lane);
                });
                
                REQUIRE(recovered.size() == 3);
                REQUIRE(recovered[0].first == annotated_haplotype[0].first);
                REQUIRE(recovered[0].second == annotated_haplotype[0].second + 1);
                // The second mapping should not get bumped up.
                REQUIRE(recovered[1].first == annotated_haplotype[1].first);
                REQUIRE(recovered[1].second == annotated_haplotype[1].second);
                REQUIRE(recovered[2].first == annotated_haplotype[2].first);
                REQUIRE(recovered[2].second == annotated_haplotype[2].second + 1);
                
            }
            
            SECTION("A haplotype without lane numbers can be appended") {
                // Make a haplotype
                vector<handle_t> hap3;
                hap3.emplace_back(net_graph.get_handle(1, false));
                hap3.emplace_back(net_graph.get_handle(2, false));
                hap3.emplace_back(net_graph.get_handle(8, false));
                
                // Put it in the state
                auto added = state.append(hap3);
                
                SECTION("The state now has 3 haplotypes") {
                    REQUIRE(state.size() == 3);
                }
                
                SECTION("The returned annotated haplotype is correct") {
                    REQUIRE(added.size() == 3);
                    REQUIRE(added[0].first == hap3[0]);
                    REQUIRE(added[0].second == 2);
                    REQUIRE(added[1].first == hap3[1]);
                    REQUIRE(added[1].second == 1);
                    REQUIRE(added[2].first == hap3[2]);
                    REQUIRE(added[2].second == 2);
                }
                
                SECTION("The new haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(2, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered == added);
                }
                
                SECTION("It can be deleted again") {
                    state.erase(2);
                    
                    REQUIRE(state.size() == 2);
                        
                    SECTION("The second haplotype can be traced back again") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(0, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered == hap2);
                    }
                    
                    SECTION("The first haplotype can be traced back again") {
                        vector<pair<handle_t, size_t>> recovered;
                        
                        state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                            recovered.emplace_back(visit, local_lane);
                        });
                        
                        REQUIRE(recovered.size() == 3);
                        REQUIRE(recovered[0].first == annotated_haplotype[0].first);
                        REQUIRE(recovered[0].second == annotated_haplotype[0].second + 1);
                        // The second mapping should not get bumped up.
                        REQUIRE(recovered[1].first == annotated_haplotype[1].first);
                        REQUIRE(recovered[1].second == annotated_haplotype[1].second);
                        REQUIRE(recovered[2].first == annotated_haplotype[2].first);
                        REQUIRE(recovered[2].second == annotated_haplotype[2].second + 1);
                        
                    }
                    
                }
                
            }
            
            SECTION("A haplotype without lane numbers can be inserted") {
                // Make a haplotype
                vector<handle_t> hap3;
                hap3.emplace_back(net_graph.get_handle(1, false));
                hap3.emplace_back(net_graph.get_handle(2, false));
                hap3.emplace_back(net_graph.get_handle(8, false));
                
                // Put it in the state, at lane 1
                auto added = state.insert(1, hap3);
                
                SECTION("The returned annotated haplotype is correct") {
                    REQUIRE(added.size() == 3);
                    REQUIRE(added[0].first == hap3[0]);
                    REQUIRE(added[0].second == 1);
                    REQUIRE(added[1].first == hap3[1]);
                    // We don't actually care about our middle node lane assignment.
                    // It could be before or after other haplotypes; they're allowed to cross over each other arbitrarily.
                    REQUIRE(added[1].second >= 0);
                    REQUIRE(added[1].second <= 1);
                    REQUIRE(added[2].first == hap3[2]);
                    REQUIRE(added[2].second == 1);
                }
                
                SECTION("The new haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(1, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered == added);
                }
                
                SECTION("The bumped-up haplotype can be traced back again") {
                    vector<pair<handle_t, size_t>> recovered;
                    
                    state.trace(2, false, [&](const handle_t& visit, size_t local_lane) {
                        recovered.emplace_back(visit, local_lane);
                    });
                    
                    REQUIRE(recovered.size() == 3);
                    REQUIRE(net_graph.get_id(recovered[0].first) == 1);
                    REQUIRE(recovered[0].second == 2);
                    REQUIRE(net_graph.get_id(recovered[1].first) == 2);
                    // Lane assignment at the middle visit may or may not have been pushed up.
                    REQUIRE(recovered[1].second >= 0);
                    REQUIRE(recovered[1].second <= 1);
                    REQUIRE(recovered[1].second != added[1].second);
                    REQUIRE(net_graph.get_id(recovered[2].first) == 8);
                    REQUIRE(recovered[2].second == 2);
                }
                
            }
        
        }
        
    }
    
    SECTION("Haplotypes cannot be added in reverse") {
        // Make a haplotype
        vector<pair<handle_t, size_t>> annotated_haplotype;
        
        // Say we go 8 rev, 2 rev (which is a child snarl), 1 rev
        annotated_haplotype.emplace_back(net_graph.get_handle(8, true), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(2, true), 0);
        annotated_haplotype.emplace_back(net_graph.get_handle(1, true), 0);
        
        // Try and fail to put it in the state
        REQUIRE_THROWS(state.insert(annotated_haplotype));
        
    }
    
    

}
    
        
}
}


















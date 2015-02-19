// The Firmament project
// Copyright (c) 2013 Malte Schwarzkopf <malte.schwarzkopf@cl.cam.ac.uk>
//
// Representation of a Quincy-style scheduling flow graph.

#ifndef FIRMAMENT_SCHEDULING_FLOW_GRAPH_H
#define FIRMAMENT_SCHEDULING_FLOW_GRAPH_H

#include <queue>
#include <string>
#include <vector>

#include "base/common.h"
#include "base/types.h"
#include "base/resource_topology_node_desc.pb.h"
#include "engine/topology_manager.h"
#include "misc/equivclasses.h"
#include "misc/map-util.h"
#include "scheduling/dimacs_change.h"
#include "scheduling/flow_graph_arc.h"
#include "scheduling/flow_graph_node.h"
#include "scheduling/flow_scheduling_cost_model_interface.h"

DECLARE_bool(preemption);

namespace firmament {

class FlowGraph {
 public:
  explicit FlowGraph(FlowSchedulingCostModelInterface* cost_model);
  virtual ~FlowGraph();
  // Public API
  void AddOrUpdateJobNodes(JobDescriptor* jd);
  void AddResourceNode(const ResourceTopologyNodeDescriptor* rtnd);
  void AddResourceTopology(
      const ResourceTopologyNodeDescriptor& resource_tree);
  void ChangeArc(FlowGraphArc* arc, uint64_t cap_lower_bound,
                 uint64_t cap_upper_bound, uint64_t cost);
  bool CheckNodeType(uint64_t node, FlowNodeType_NodeType type);
  void DeleteTaskNode(TaskID_t task_id);
  void DeleteResourceNode(ResourceID_t res_id);
  void DeleteNodesForJob(JobID_t job_id);
  FlowGraphNode* GetUnschedAggForJob(JobID_t job_id);
  FlowGraphNode* NodeForResourceID(const ResourceID_t& res_id);
  FlowGraphNode* NodeForTaskID(TaskID_t task_id);
  void ResetChanges();
  void UpdateArcsForBoundTask(TaskID_t tid, ResourceID_t res_id);
  void UpdateResourceNode(const ResourceTopologyNodeDescriptor* rtnd);
  void UpdateResourceTopology(
      const ResourceTopologyNodeDescriptor& resource_tree);

  // Simple accessor methods
  inline const unordered_set<FlowGraphArc*>& Arcs() const { return arc_set_; }
  inline const unordered_map<uint64_t, FlowGraphNode*>& Nodes() const {
    return node_map_;
  }
  inline const unordered_set<uint64_t>& leaf_node_ids() const {
    return leaf_nodes_;
  }
  inline const unordered_set<uint64_t>& task_node_ids() const {
    return task_nodes_;
  }
  inline const unordered_set<uint64_t>& unsched_agg_ids() const {
    return unsched_agg_nodes_;
  }
  inline const FlowGraphNode& sink_node() const { return *sink_node_; }
  inline const FlowGraphNode& cluster_agg_node() const {
    return *cluster_agg_node_;
  }
  inline uint64_t NumArcs() const { return arc_set_.size(); }
  //inline uint64_t NumNodes() const { return node_map_.size(); }
  inline uint64_t NumNodes() const { return current_id_ - 1; }
  inline FlowGraphNode* Node(uint64_t id) {
    FlowGraphNode* const* npp = FindOrNull(node_map_, id);
    return (npp ? *npp : NULL);
  }
  inline vector<DIMACSChange*>& graph_changes() { return graph_changes_; }

 protected:
  FRIEND_TEST(DIMACSExporterTest, LargeGraph);
  FRIEND_TEST(DIMACSExporterTest, ScalabilityTestGraphs);
  FRIEND_TEST(FlowGraphTest, AddArcToNode);
  FRIEND_TEST(FlowGraphTest, AddOrUpdateJobNodes);
  FRIEND_TEST(FlowGraphTest, AddResourceNode);
  FRIEND_TEST(FlowGraphTest, ChangeArc);
  FRIEND_TEST(FlowGraphTest, DeleteTaskNode);
  FRIEND_TEST(FlowGraphTest, DeleteResourceNode);
  FRIEND_TEST(FlowGraphTest, DeleteNodesForJob);
  FRIEND_TEST(FlowGraphTest, ResetChanges);
  FRIEND_TEST(FlowGraphTest, UnschedAggCapacityAdjustment);
  void AddArcsForTask(FlowGraphNode* task_node, FlowGraphNode* unsched_agg_node,
                      vector<FlowGraphArc*>* task_arcs);
  FlowGraphArc* AddArcInternal(FlowGraphNode* src, FlowGraphNode* dst);
  FlowGraphNode* AddNodeInternal(uint64_t id);
  FlowGraphArc* AddArcInternal(uint64_t src, uint64_t dst);
  FlowGraphNode* AddEquivClassAggregator(const TaskDescriptor& td,
                                         vector<FlowGraphArc*>* ec_arcs);
  void AddEquivClassPreferenceArcs(const TaskDescriptor& td,
                                   FlowGraphNode* equiv_node,
                                   vector<FlowGraphArc*>* ec_arcs);
  void AddSpecialNodes();
  void AdjustUnscheduledAggToSinkCapacityGeneratingDelta(
      JobID_t job, int64_t delta);
  void ConfigureResourceRootNode(const ResourceTopologyNodeDescriptor& rtnd,
                                 FlowGraphNode* new_node);
  void ConfigureResourceBranchNode(const ResourceTopologyNodeDescriptor& rtnd,
                                   FlowGraphNode* new_node);
  void ConfigureResourceLeafNode(const ResourceTopologyNodeDescriptor& rtnd,
                                 FlowGraphNode* new_node);
  void DeleteArcGeneratingDelta(FlowGraphArc* arc);
  void DeleteArc(FlowGraphArc* arc);
  void DeleteNode(FlowGraphNode* node);
  void DeleteOrUpdateTaskEquivNode(TaskID_t task_id);
  void PinTaskToNode(FlowGraphNode* task_node, FlowGraphNode* res_node);

  uint64_t next_id() {
    if (unused_ids_.empty()) {
      ids_created_.push_back(current_id_);
      return current_id_++;
    } else {
      uint64_t new_id = unused_ids_.front();
      unused_ids_.pop();
      ids_created_.push_back(new_id);
      return new_id;
    }
  }

  // Flow scheduling cost model used
  FlowSchedulingCostModelInterface* cost_model_;

  // Graph structure containers and helper fields
  uint64_t current_id_;
  unordered_map<uint64_t, FlowGraphNode*> node_map_;
  unordered_set<FlowGraphArc*> arc_set_;
  FlowGraphNode* cluster_agg_node_;
  FlowGraphNode* sink_node_;
  // Resource and task mappings
  unordered_map<TaskID_t, uint64_t> task_to_nodeid_map_;
  unordered_map<ResourceID_t, uint64_t,
      boost::hash<boost::uuids::uuid> > resource_to_nodeid_map_;
  // Hacky solution for retrieval of the parent of any particular resource
  // (needed to assign capacities properly by back-tracking).
  unordered_map<ResourceID_t, ResourceID_t,
      boost::hash<boost::uuids::uuid> > resource_to_parent_map_;
  // Hacky equivalence class node map
  unordered_map<TaskID_t, uint64_t> task_to_equiv_class_node_id_;
  // The "node ID" for the job is currently the ID of the job's unscheduled node
  unordered_map<JobID_t, uint64_t,
      boost::hash<boost::uuids::uuid> > job_unsched_to_node_id_;
  unordered_set<uint64_t> leaf_nodes_;
  unordered_set<uint64_t> task_nodes_;
  unordered_set<uint64_t> unsched_agg_nodes_;

  // Vector storing the graph changes occured since the last scheduling round.
  vector<DIMACSChange*> graph_changes_;
  // Queue storing the ids of the nodes we've previously removed.
  queue<uint64_t> unused_ids_;
  // Vector storing the ids of the nodes we've created.
  vector<uint64_t> ids_created_;

  uint32_t rand_seed_ = 0;
};

}  // namespace firmament

#endif  // FIRMAMENT_SCHEDULING_FLOW_GRAPH_H

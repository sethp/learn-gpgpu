// Copyright (c) 2018 the Talvos developers. All rights reserved.
//
// This file is distributed under a three-clause BSD license. For full license
// terms please see the LICENSE file distributed with this source code.

/// \file GraphicsPipeline.h
/// This file declares the GraphicsPipeline class.

#ifndef TALVOS_GRAPHICSPIPELINE_H
#define TALVOS_GRAPHICSPIPELINE_H

#include <array>
#include <vector>

#include "vulkan/vulkan_core.h"

namespace talvos
{

class PipelineStage;

/// A list of vertex attribute descriptions.
typedef std::vector<VkVertexInputAttributeDescription>
    VertexAttributeDescriptionList;

/// A list of vertex binding descriptions.
typedef std::vector<VkVertexInputBindingDescription>
    VertexBindingDescriptionList;

/// A list of pipeline color blend attachment states.
typedef std::vector<VkPipelineColorBlendAttachmentState>
    BlendAttachmentStateList;

/// This class encapsulates a graphics pipeline.
class GraphicsPipeline
{
public:
  /// Create a graphics pipeline.
  /// Ownership of any non-null stages is transferred to the pipeline.
  GraphicsPipeline(
      VkPrimitiveTopology Topology, PipelineStage *VertexStage,
      PipelineStage *FragmentStage,
      const VertexBindingDescriptionList &VertexBindingDescriptions,
      const VertexAttributeDescriptionList &VertexAttributeDescriptions,
      const VkPipelineRasterizationStateCreateInfo &RasterizationState,
      const BlendAttachmentStateList &BlendAttachmentStates,
      const std::array<float, 4> &BlendConstants,
      const std::vector<VkViewport> &Viewports,
      const std::vector<VkRect2D> &Scissors)
      : Topology(Topology), VertexStage(VertexStage),
        FragmentStage(FragmentStage),
        VertexBindingDescriptions(VertexBindingDescriptions),
        VertexAttributeDescriptions(VertexAttributeDescriptions),
        RasterizationState(RasterizationState),
        BlendAttachmentStates(BlendAttachmentStates),
        BlendConstants(BlendConstants), Viewports(Viewports),
        Scissors(Scissors){};

  /// Destroy the pipeline.
  ~GraphicsPipeline();

  // Do not allow GraphicsPipeline objects to be copied.
  ///\{
  GraphicsPipeline(const GraphicsPipeline &) = delete;
  GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
  ///\}

  /// Returns the list of blend attachment states.
  const BlendAttachmentStateList &getBlendAttachmentStates() const
  {
    return BlendAttachmentStates;
  }

  /// Returns the blend constants.
  const std::array<float, 4> &getBlendConstants() const
  {
    return BlendConstants;
  }

  /// Returns the fragment pipeline stage.
  const PipelineStage *getFragmentStage() const { return FragmentStage; }

  /// Returns the rasterization state used by this pipeline.
  const VkPipelineRasterizationStateCreateInfo &getRasterizationState() const
  {
    return RasterizationState;
  }

  /// Returns the static scissor rectangles used by this pipeline.
  const std::vector<VkRect2D> &getScissors() const { return Scissors; }

  /// Returns the primitive topology used by this pipeline.
  VkPrimitiveTopology getTopology() const { return Topology; }

  /// Returns the vertex pipeline stage.
  const PipelineStage *getVertexStage() const { return VertexStage; }

  /// Returns the list of vertex attribute descriptions.
  const VertexAttributeDescriptionList &getVertexAttributeDescriptions() const
  {
    return VertexAttributeDescriptions;
  }

  /// Returns the list of vertex binding descriptions.
  const VertexBindingDescriptionList &getVertexBindingDescriptions() const
  {
    return VertexBindingDescriptions;
  }

  /// Returns the static viewports used by this pipeline.
  const std::vector<VkViewport> &getViewports() const { return Viewports; }

private:
  /// The primitive topology used by this pipeline.
  VkPrimitiveTopology Topology;

  /// The vertex pipeline stage in this pipeline.
  PipelineStage *VertexStage;

  /// The fragment pipeline stage in this pipeline.
  PipelineStage *FragmentStage;

  /// The vertex binding descriptions.
  VertexBindingDescriptionList VertexBindingDescriptions;

  /// The vertex attribute descriptions.
  VertexAttributeDescriptionList VertexAttributeDescriptions;

  /// The rasterization state.
  VkPipelineRasterizationStateCreateInfo RasterizationState;

  /// The color blend attachement states.
  BlendAttachmentStateList BlendAttachmentStates;

  /// The blend constants.
  std::array<float, 4> BlendConstants;

  /// The static viewports used by this pipeline.
  std::vector<VkViewport> Viewports;

  /// The static scissor rectangles used by this pipeline.
  std::vector<VkRect2D> Scissors;
};

} // namespace talvos

#endif

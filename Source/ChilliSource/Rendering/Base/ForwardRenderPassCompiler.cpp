//
//  The MIT License (MIT)
//
//  Copyright (c) 2016 Tag Games Limited
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.
//

#include <ChilliSource/Rendering/Base/ForwardRenderPassCompiler.h>

#include <ChilliSource/Core/Math/Geometry/ShapeIntersection.h>
#include <ChilliSource/Core/Threading/TaskScheduler.h>
#include <ChilliSource/Rendering/Base/ForwardRenderPasses.h>
#include <ChilliSource/Rendering/Base/RenderFrame.h>
#include <ChilliSource/Rendering/Base/RenderObject.h>
#include <ChilliSource/Rendering/Base/RenderPass.h>
#include <ChilliSource/Rendering/Base/RenderPassObject.h>
#include <ChilliSource/Rendering/Base/RenderPassObjectSorter.h>
#include <ChilliSource/Rendering/Base/RenderPassVisibilityChecker.h>
#include <ChilliSource/Rendering/Model/RenderDynamicMesh.h>


namespace ChilliSource
{
    namespace
    {
        /// Converts the given RenderObject to a RenderPassObject using the given RenderMaterial.
        /// if the given RenderMaterial does not exist in the RenderMaterialGroup contained by
        /// the RenderObject, then this will assert.
        ///
        /// @param renderObject
        ///     The renderObject to convert.
        /// @param renderMaterial
        ///     The render material that should be used by the new RenderPassObject.
        ///
        /// @return The new RenderPassObject.
        ///
        RenderPassObject ConvertToRenderPassObject(const RenderObject& renderObject, const RenderMaterial* renderMaterial) noexcept
        {
            CS_ASSERT(renderObject.GetRenderMaterialGroup()->Contains(renderMaterial), "Invalid render material.");
            
            switch (renderObject.GetType())
            {
                case RenderObject::Type::k_static:
                    return RenderPassObject(renderMaterial, renderObject.GetRenderMesh(), renderObject.GetWorldMatrix(), renderObject.GetBoundingSphere(), renderObject.GetPriority());
                case RenderObject::Type::k_dynamic:
                    return RenderPassObject(renderMaterial, renderObject.GetRenderDynamicMesh(), renderObject.GetWorldMatrix(), renderObject.GetBoundingSphere(), renderObject.GetPriority());
                default:
                    CS_LOG_FATAL("Invalid RenderObject type.");
                    return RenderPassObject(nullptr, reinterpret_cast<const RenderMesh*>(NULL), Matrix4::k_identity, Sphere(), 0);
            }
        }
        
        /// Returns the vertex format of the given render object.
        ///
        /// @param renderObject
        ///     The render object.
        ///
        /// @return The vertex format.
        ///
        const VertexFormat& GetVertexFormat(const RenderObject& renderObject)
        {
            switch (renderObject.GetType())
            {
                case RenderObject::Type::k_static:
                    return renderObject.GetRenderMesh()->GetVertexFormat();
                case RenderObject::Type::k_dynamic:
                    return renderObject.GetRenderDynamicMesh()->GetVertexFormat();
                default:
                    CS_LOG_FATAL("Invalid RenderObject type.");
                    return renderObject.GetRenderMesh()->GetVertexFormat();
            }
        }
        
        /// Calculate the number of targets
        ///
        /// @param renderFrame
        ///     The render frame from which to calculate the numer of passes.
        ///
        /// @return The number of passes.
        ///
        u32 CalcNumTargets(const RenderFrame& renderFrame) noexcept
        {
            // The main target
            constexpr u32 k_reservedTargets = 1;
            
            u32 numShadowCastingLights = 0;
            for (const auto& directionalRenderLight : renderFrame.GetDirectionalRenderLights())
            {
                if (directionalRenderLight.GetShadowMapTarget())
                {
                    ++numShadowCastingLights;
                }
            }
            
            return numShadowCastingLights;
        }
        
        /// Calculates main scene passes in the given render frame.
        ///
        /// @param renderFrame
        ///     The render frame from which to calculate the numer of passes.
        ///
        /// @return The number of passes.
        ///
        u32 CalcNumScenePasses(const RenderFrame& renderFrame) noexcept
        {
            // Base + Transparent
            constexpr u32 k_reservedRenderPasses = 2;
            
            u32 numDirectionalLightPasses = u32(renderFrame.GetDirectionalRenderLights().size());
            u32 numPointLightPasses = u32(renderFrame.GetPointRenderLights().size());
            
            return k_reservedRenderPasses + numDirectionalLightPasses + numPointLightPasses;
        }
        
        /// Filters the given list of objects to return only the objects which are a part of the requested
        /// layer.
        ///
        /// @param renderLayer
        ///     The render layer to filter on.
        /// @param renderObjects
        ///     The list of render objects which should be filtered.
        ///
        /// @return The list of render objects for the requested layer.
        ///
        std::vector<RenderObject> GetLayerRenderObjects(RenderLayer renderLayer, const std::vector<RenderObject>& renderObjects) noexcept
        {
            std::vector<RenderObject> layerRenderObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                if (renderObject.GetRenderLayer() == renderLayer)
                {
                    layerRenderObjects.push_back(renderObject);
                }
            }
            
            return layerRenderObjects;
        }
        
        /// Parses a list of RenderObjects and generates a list of RenderPassObjects for
        /// each RenderObject that has a ShadowMap pass defined, and has shadow casting
        /// enabled.
        ///
        /// @param renderObjects
        ///     A list of RenderObjects to parse
        ///
        /// @return The collection of RenderPassObjects.
        ///
        std::vector<RenderPassObject> GetShadowMapRenderPassObjects(const std::vector<RenderObject>& renderObjects) noexcept
        {
            std::vector<RenderPassObject> baseRenderPassObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                if (renderObject.ShouldCastShadows())
                {
                    auto renderMaterial = renderObject.GetRenderMaterialGroup()->GetRenderMaterial(GetVertexFormat(renderObject), static_cast<u32>(ForwardRenderPasses::k_shadowMap));
                    
                    if (renderMaterial)
                    {
                        baseRenderPassObjects.push_back(ConvertToRenderPassObject(renderObject, renderMaterial));
                    }
                }
            }
            
            return baseRenderPassObjects;
        }
        
        /// Parses a list of RenderObjects and generates a list of RenderPassObjects for
        /// each RenderObject that has a Base pass defined.
        ///
        /// @param renderObjects
        ///     A list of RenderObjects to parse
        ///
        /// @return A collection of RenderPassObjects, one for each RenderObject Base pass
        ///
        std::vector<RenderPassObject> GetBaseRenderPassObjects(const std::vector<RenderObject>& renderObjects) noexcept
        {
            std::vector<RenderPassObject> baseRenderPassObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                auto renderMaterial = renderObject.GetRenderMaterialGroup()->GetRenderMaterial(GetVertexFormat(renderObject), static_cast<u32>(ForwardRenderPasses::k_base));
                
                if (renderMaterial)
                {
                    baseRenderPassObjects.push_back(ConvertToRenderPassObject(renderObject, renderMaterial));
                }
            }
            
            return baseRenderPassObjects;
        }
        
        /// Parses a list of RenderObjects and generates a list of RenderPassObjects for
        /// each RenderObject that has either a DirectionalLight or DirectionalLightShadows
        /// pass defined, depending on the type of directional light.
        ///
        /// @param renderObjects
        ///     A list of RenderObjects to parse
        /// @param directionalRenderLight
        ///     The directional light to get objects for.
        ///
        /// @return A collection of RenderPassObjects, one for each RenderObject Directional pass
        ///
        std::vector<RenderPassObject> GetDirectionalLightRenderPassObjects(const std::vector<RenderObject>& renderObjects, const DirectionalRenderLight& directionalRenderLight) noexcept
        {
            ForwardRenderPasses passType = ForwardRenderPasses::k_directionalLight;
            if (directionalRenderLight.GetShadowMapTarget())
            {
                passType = ForwardRenderPasses::k_directionalLightShadows;
            }
            
            std::vector<RenderPassObject> renderPassObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                auto renderMaterial = renderObject.GetRenderMaterialGroup()->GetRenderMaterial(GetVertexFormat(renderObject), static_cast<u32>(passType));
                
                if (renderMaterial)
                {
                    renderPassObjects.push_back(ConvertToRenderPassObject(renderObject, renderMaterial));
                }
            }
            
            return renderPassObjects;
        }
        
        /// Parses a list of RenderObjects and generates a list of RenderPassObjects for
        /// each RenderObject that has a PointLight pass defined, and is within the range
        /// of influence of the given light.
        ///
        /// @param renderObjects
        ///     A list of RenderObjects to parse
        /// @param pointRenderLight
        ///     The render light to get objects for.
        ///
        /// @return A collection of RenderPassObjects for the render light pass.
        ///
        std::vector<RenderPassObject> GetPointLightRenderPassObjects(const std::vector<RenderObject>& renderObjects, const PointRenderLight& pointRenderLight) noexcept
        {
            Sphere pointLightBoundingSphere(pointRenderLight.GetPosition(), pointRenderLight.GetRangeOfInfluence());
            
            std::vector<RenderPassObject> renderPassObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                auto renderMaterial = renderObject.GetRenderMaterialGroup()->GetRenderMaterial(GetVertexFormat(renderObject), static_cast<u32>(ForwardRenderPasses::k_pointLight));
                
                if (renderMaterial && pointLightBoundingSphere.Contains(renderObject.GetBoundingSphere()))
                {
                    renderPassObjects.push_back(ConvertToRenderPassObject(renderObject, renderMaterial));
                }
            }
            
            return renderPassObjects;
        }
        
        /// Parses a list of RenderObjects and generates a list of RenderPassObjects for
        /// each RenderObject that has a Transparent pass defined.
        ///
        /// @param renderObjects
        ///     A list of RenderObjects to parse
        ///
        /// @return A collection of RenderPassObjects, one for each RenderObject Transparent pass
        ///
        std::vector<RenderPassObject> GetTransparentRenderPassObjects(const std::vector<RenderObject>& renderObjects) noexcept
        {
            std::vector<RenderPassObject> transparentRenderPassObjects;
            
            for (const auto& renderObject : renderObjects)
            {
                auto renderMaterial = renderObject.GetRenderMaterialGroup()->GetRenderMaterial(GetVertexFormat(renderObject), static_cast<u32>(ForwardRenderPasses::k_transparent));
                
                if (renderMaterial)
                {
                    transparentRenderPassObjects.push_back(ConvertToRenderPassObject(renderObject, renderMaterial));
                }
            }
            
            return transparentRenderPassObjects;
        }
        
        /// Gather all render objects in the frame that are to be renderered into the default RenderTarget
        /// and parse them into different RenderPasses for each light source plus the required Base and
        /// Transparent passes. These passes are then compiled into a CameraRenderPassGroup.
        ///
        /// @param taskContext
        ///     Context to manage any spawned tasks
        /// @param renderFrame
        ///     Current frame data
        ///
        /// @return The CameraRenderPassGroup
        ///
        CameraRenderPassGroup CompleSceneCameraRenderPassGroup(const TaskContext& taskContext, const RenderFrame& renderFrame) noexcept
        {
            auto standardRenderObjects = GetLayerRenderObjects(RenderLayer::k_standard, renderFrame.GetRenderObjects());
            auto visibleStandardRenderObjects = RenderPassVisibilityChecker::CalculateVisibleObjects(taskContext, renderFrame.GetRenderCamera(), standardRenderObjects);
            
            u32 numPasses = CalcNumScenePasses(renderFrame);
            std::vector<RenderPass> renderPasses(numPasses);
            std::vector<Task> tasks;
            u32 nextPassIndex = 0;
            
            // Base pass
            u32 basePassIndex = nextPassIndex++;
            tasks.push_back([=, &renderPasses, &renderFrame, &visibleStandardRenderObjects](const TaskContext& innerTaskContext)
            {
                auto renderPassObjects = GetBaseRenderPassObjects(visibleStandardRenderObjects);
                RenderPassObjectSorter::OpaqueSort(renderFrame.GetRenderCamera(), renderPassObjects);
                renderPasses[basePassIndex] = RenderPass(renderFrame.GetAmbientRenderLight(), renderPassObjects);
            });
            
            // Directional light pass
            for (const auto& directionalLight : renderFrame.GetDirectionalRenderLights())
            {
                u32 directionLightPassIndex = nextPassIndex++;
                tasks.push_back([=, &renderPasses, &renderFrame, &visibleStandardRenderObjects, &directionalLight](const TaskContext& innerTaskContext)
                {
                    auto renderPassObjects = GetDirectionalLightRenderPassObjects(visibleStandardRenderObjects, directionalLight);
                    RenderPassObjectSorter::OpaqueSort(renderFrame.GetRenderCamera(), renderPassObjects);
                    renderPasses[directionLightPassIndex] = RenderPass(directionalLight, renderPassObjects);
                });
            }
            
            // Point light pass
            for (const auto& pointLight : renderFrame.GetPointRenderLights())
            {
                u32 pointLightPassIndex = nextPassIndex++;
                tasks.push_back([=, &renderPasses, &renderFrame, &visibleStandardRenderObjects, &pointLight](const TaskContext& innerTaskContext)
                {
                    auto renderPassObjects = GetPointLightRenderPassObjects(visibleStandardRenderObjects, pointLight);
                    RenderPassObjectSorter::OpaqueSort(renderFrame.GetRenderCamera(), renderPassObjects);
                    renderPasses[pointLightPassIndex] = RenderPass(pointLight, renderPassObjects);
                });
            }
            
            // Transparent pass
            u32 transparentPassIndex = nextPassIndex++;
            tasks.push_back([=, &renderPasses, &renderFrame, &visibleStandardRenderObjects](const TaskContext& innerTaskContext)
            {
                auto renderPassObjects = GetTransparentRenderPassObjects(visibleStandardRenderObjects);
                RenderPassObjectSorter::TransparentSort(renderFrame.GetRenderCamera(), renderPassObjects);
                renderPasses[transparentPassIndex] = RenderPass(renderFrame.GetAmbientRenderLight(), renderPassObjects);
            });
            
            taskContext.ProcessChildTasks(tasks);
            
            return CameraRenderPassGroup(renderFrame.GetRenderCamera(), renderPasses);
        }
        
        /// Gathers all UI render objects in the frame that are to be rendered to the default Render Target
        /// and compiles them into RenderPasses. These render passes are then compiled into a
        /// CameraRenderPassGroup which uses the UI camera.
        ///
        /// @param taskContext
        ///     Context to manage any spawned tasks
        /// @param renderFrame
        ///     Current frame data
        ///
        /// @return The generated CameraRenderPassGroup
        ///
        CameraRenderPassGroup CompileUICameraRenderPassGroup(const TaskContext& taskContext, const RenderFrame& renderFrame) noexcept
        {
            constexpr f32 k_near = 0.0f;
            constexpr f32 k_far = 1.0f;
            auto projMatrix = Matrix4::CreateOrthographicProjectionLH(0, renderFrame.GetResolution().x, 0, renderFrame.GetResolution().y, k_near, k_far);
            RenderCamera uiCamera(Matrix4::k_identity, projMatrix, Quaternion::k_identity);
            
            auto uiRenderObjects = GetLayerRenderObjects(RenderLayer::k_ui, renderFrame.GetRenderObjects());
            auto visibleUIRenderObjects = RenderPassVisibilityChecker::CalculateVisibleObjects(taskContext, uiCamera, uiRenderObjects);
            
            auto uiRenderPassObjects = GetTransparentRenderPassObjects(visibleUIRenderObjects);
            CS_ASSERT(visibleUIRenderObjects.size() == uiRenderPassObjects.size(), "Invalid number of render pass objects in transparent pass. All render objects in the UI layer should have a transparent material.");
            
            RenderPassObjectSorter::PrioritySort(uiRenderPassObjects);
            
            std::vector<RenderPass> renderPasses;
            if (uiRenderPassObjects.size() > 0)
            {
                renderPasses.push_back(RenderPass(renderFrame.GetAmbientRenderLight(), uiRenderPassObjects));
            }
            
            return CameraRenderPassGroup(uiCamera, renderPasses);
        }
        
        /// Gather all render objects in the frame that are to be renderered into the default RenderTarget
        /// into a TargetRenderPassGroup.
        ///
        /// @param taskContext
        ///     Context to manage any spawned tasks
        /// @param renderFrame
        ///     Current frame data
        ///
        /// @return The TargetRenderPassGroup
        ///
        TargetRenderPassGroup CompileMainTargetRenderPassGroup(const TaskContext& taskContext, const RenderFrame& renderFrame) noexcept
        {
            constexpr u32 k_numGroups = 2;
            
            std::vector<CameraRenderPassGroup> cameraRenderPassGroups(k_numGroups);
            std::vector<Task> tasks;
            u32 nextIndex = 0;
            
            // Scene camera group
            u32 sceneIndex = nextIndex++;
            tasks.push_back([=, &cameraRenderPassGroups, &renderFrame](const TaskContext& innerTaskContext)
            {
                cameraRenderPassGroups[sceneIndex] = CompleSceneCameraRenderPassGroup(innerTaskContext, renderFrame);
            });
            
            // UI Camera group
            u32 uiIndex = nextIndex++;
            tasks.push_back([=, &cameraRenderPassGroups, &renderFrame](const TaskContext& innerTaskContext)
            {
                cameraRenderPassGroups[uiIndex] = CompileUICameraRenderPassGroup(innerTaskContext, renderFrame);
            });
            
            taskContext.ProcessChildTasks(tasks);
            
            return TargetRenderPassGroup(cameraRenderPassGroups);
        }
        
        /// Gather all render objects in the frame that are to be renderered into a shadow RenderTarget
        /// into a TargetRenderPassGroup.
        ///
        /// @param taskContext
        ///     Context to manage any spawned tasks
        /// @param renderFrame
        ///     Current frame data
        /// @param directionalRenderLight
        ///     The directional light that should have a shadow map built for it.
        ///
        /// @return The TargetRenderPassGroup
        ///
        TargetRenderPassGroup CompileShadowMapTargetRenderPassGroup(const TaskContext& taskContext, const RenderFrame& renderFrame, const DirectionalRenderLight& directionalRenderLight) noexcept
        {
            CS_ASSERT(directionalRenderLight.GetShadowMapTarget(), "Cannot compile shadow map target with light that has no shadow map target.");
            
            //TODO: Correctly setup camera.
            RenderCamera camera;
            
            auto standardRenderObjects = GetLayerRenderObjects(RenderLayer::k_standard, renderFrame.GetRenderObjects());
            auto visibleStandardRenderObjects = RenderPassVisibilityChecker::CalculateVisibleObjects(taskContext, renderFrame.GetRenderCamera(), standardRenderObjects);
            auto renderPassObjects = GetShadowMapRenderPassObjects(visibleStandardRenderObjects);
            RenderPassObjectSorter::OpaqueSort(renderFrame.GetRenderCamera(), renderPassObjects);
            auto renderPass = RenderPass(renderPassObjects);
            
            CameraRenderPassGroup camerRenderPassGroup(camera, { renderPass });
            return TargetRenderPassGroup(directionalRenderLight.GetShadowMapTarget(), { camerRenderPassGroup });
        }
    }
    
    //------------------------------------------------------------------------------
    std::vector<TargetRenderPassGroup> ForwardRenderPassCompiler::CompileTargetRenderPassGroups(const TaskContext& taskContext, const RenderFrame& renderFrame) noexcept
    {
        auto numTargets = CalcNumTargets(renderFrame);
        std::vector<TargetRenderPassGroup> targetRenderPassGroups(numTargets);
        std::vector<Task> tasks;
        u32 nextPassIndex = 0;
        
        // Shadow targets
        for (const auto& directionalRenderLight : renderFrame.GetDirectionalRenderLights())
        {
            if (directionalRenderLight.GetShadowMapTarget())
            {
                u32 shadowPassIndex = nextPassIndex++;
                tasks.push_back([=, &targetRenderPassGroups, &renderFrame, &directionalRenderLight](const TaskContext& innerTaskContext)
                {
                    targetRenderPassGroups[shadowPassIndex] = CompileShadowMapTargetRenderPassGroup(innerTaskContext, renderFrame, directionalRenderLight);
                });
            }
        }
        
        // Main target
        u32 mainPassIndex = nextPassIndex++;
        tasks.push_back([=, &targetRenderPassGroups, &renderFrame](const TaskContext& innerTaskContext)
        {
            targetRenderPassGroups[mainPassIndex] = CompileMainTargetRenderPassGroup(innerTaskContext, renderFrame);
        });
        
        taskContext.ProcessChildTasks(tasks);
        
        return targetRenderPassGroups;
    }
} 
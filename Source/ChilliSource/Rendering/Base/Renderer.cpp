/*
 *  Renderer.cpp
 *  moFlo
 *
 *  Created by Scott Downie on 30/09/2010.
 *  Copyright 2010 Tag Games. All rights reserved.
 *
 */

#include <ChilliSource/Rendering/Base/Renderer.h>
#include <ChilliSource/Rendering/Base/RenderSystem.h>

#include <ChilliSource/Rendering/Texture/Texture.h>
#include <ChilliSource/Rendering/Base/RenderComponent.h>
#include <ChilliSource/Rendering/Camera/CameraComponent.h>
#include <ChilliSource/Rendering/Lighting/LightComponent.h>
#include <ChilliSource/Rendering/Lighting/DirectionalLightComponent.h>
#include <ChilliSource/Rendering/Lighting/PointLightComponent.h>
#include <ChilliSource/Rendering/Lighting/AmbientLightComponent.h>
#include <ChilliSource/Rendering/Base/RenderTarget.h>
#include <ChilliSource/Rendering/Base/RendererSortPredicates.h>
#include <ChilliSource/Rendering/Base/CullingPredicates.h>
#include <ChilliSource/Rendering/Material/MaterialFactory.h>

#include <ChilliSource/Core/Base/Application.h>
#include <ChilliSource/Core/Base/MakeDelegate.h>
#include <ChilliSource/Core/Math/Geometry/ShapeIntersection.h>
#include <ChilliSource/GUI/Base/Window.h>

#include <algorithm>

namespace ChilliSource
{
	namespace Rendering
	{
        //---Matrix caches
        Core::Matrix4x4 Renderer::matViewProjCache;
		
		typedef std::function<bool(RenderComponent*, RenderComponent*)> RenderSortDelegate;
		
        //-------------------------------------------------------
        //-------------------------------------------------------
        RendererUPtr Renderer::Create(RenderSystem* in_renderSystem)
        {
            return RendererUPtr(new Renderer(in_renderSystem));
        }
		//----------------------------------------------------------
		//----------------------------------------------------------
		Renderer::Renderer(RenderSystem* in_renderSystem)
        : mpRenderSystem(in_renderSystem)
        , mCanvas(in_renderSystem)
        , mpActiveCamera(nullptr)
		{

		}
        //----------------------------------------------------------
        //----------------------------------------------------------
        void Renderer::Init()
        {
            mpTransparentSortPredicate = RendererSortPredicateSPtr(new BackToFrontSortPredicate());
            mpOpaqueSortPredicate = RendererSortPredicateSPtr(new MaterialSortPredicate());
            
            mpPerspectiveCullPredicate = ICullingPredicateSPtr(new FrustumCullPredicate());
            mpOrthoCullPredicate = ICullingPredicateSPtr(new ViewportCullPredicate());
            
            mCanvas.Init();
        }
		//----------------------------------------------------------
		/// Set Transparent Sort Predicate
		//----------------------------------------------------------
		void Renderer::SetTransparentSortPredicate(const RendererSortPredicateSPtr & inpFunctor)
        {
			mpTransparentSortPredicate = inpFunctor;
		}
        //----------------------------------------------------------
        /// Set Opaque Sort Predicate
        //----------------------------------------------------------
        void Renderer::SetOpaqueSortPredicate(const RendererSortPredicateSPtr & inpFunctor)
        {
            mpOpaqueSortPredicate = inpFunctor;
        }
        //----------------------------------------------------------
        /// Set Perspective Cull Predicate
        //----------------------------------------------------------
        void Renderer::SetPerspectiveCullPredicate(const ICullingPredicateSPtr & inpFunctor)
        {
            mpPerspectiveCullPredicate = inpFunctor;
        }
        //----------------------------------------------------------
        /// Set Ortho Cull Predicate
        //----------------------------------------------------------
        void Renderer::SetOrthoCullPredicate(const ICullingPredicateSPtr & inpFunctor)
        {
            mpOrthoCullPredicate = inpFunctor;
        }
		//----------------------------------------------------------
		/// Get Active Camera Pointer
		//----------------------------------------------------------
		CameraComponent* Renderer::GetActiveCameraPtr()
		{
			return mpActiveCamera;
		}
		//----------------------------------------------------------
		/// Render To Screen
		//----------------------------------------------------------
		void Renderer::RenderToScreen(Core::Scene* inpScene)
		{
            RenderSceneToTarget(inpScene, mpRenderSystem->GetDefaultRenderTarget());
		}
        //----------------------------------------------------------
        /// Render To Texture
        //----------------------------------------------------------
        void Renderer::RenderToTexture(Core::Scene* inpScene, const TextureSPtr& inpColourTarget, const TextureSPtr& inpDepthTarget)
		{
            //get the width and height
            u32 udwWidth = 1;
            u32 udwHeight = 1;
            if (inpColourTarget != nullptr)
            {
                udwWidth = inpColourTarget->GetWidth();
                udwHeight = inpColourTarget->GetHeight();
            }
            else if (inpDepthTarget != nullptr)
            {
                udwWidth = inpDepthTarget->GetWidth();
                udwHeight = inpDepthTarget->GetHeight();
            }
            
            RenderTarget* pOffscreenTarget = mpRenderSystem->CreateRenderTarget(udwWidth, udwHeight);
            pOffscreenTarget->SetTargetTextures(inpColourTarget, inpDepthTarget);
            RenderSceneToTarget(inpScene, pOffscreenTarget);
            CS_SAFEDELETE(pOffscreenTarget);
		}
        //----------------------------------------------------------
		/// Render Scene To Target
		//----------------------------------------------------------
		void Renderer::RenderSceneToTarget(Core::Scene* inpScene, RenderTarget* inpRenderTarget)
        {
			//Traverse the scene graph and get all renderable objects
            std::vector<RenderComponent*> aPreFilteredRenderCache;
            std::vector<CameraComponent*> aCameraCache;
            std::vector<DirectionalLightComponent*> aDirLightCache;
            std::vector<PointLightComponent*> aPointLightCache;
            AmbientLightComponent* pAmbientLight = nullptr;
            
			FindRenderableObjectsInScene(inpScene, aPreFilteredRenderCache, aCameraCache, aDirLightCache, aPointLightCache, pAmbientLight);
            mpActiveCamera = (aCameraCache.empty() ? nullptr : aCameraCache.back());
            
            if(mpActiveCamera)
            {
                //Apply the world view projection matrix
                mpRenderSystem->ApplyCamera(mpActiveCamera->GetEntity()->GetTransform().GetWorldPosition(), mpActiveCamera->GetView(), mpActiveCamera->GetProjection(), mpActiveCamera->GetClearColour());
                //Calculate the view-projection matrix as we will need it for sorting
                Core::Matrix4x4::Multiply(&mpActiveCamera->GetView(), &mpActiveCamera->GetProjection(), &matViewProjCache);
                
                //Render shadow maps
                RenderShadowMap(mpActiveCamera, aDirLightCache, aPreFilteredRenderCache);
                
                //Cull items based on camera
                std::vector<RenderComponent*> aCameraRenderCache;
                std::vector<RenderComponent*> aCameraOpaqueCache;
                std::vector<RenderComponent*> aCameraTransparentCache;
                CullRenderables(mpActiveCamera, aPreFilteredRenderCache, aCameraRenderCache);
                FilterSceneRenderables(aCameraRenderCache, aCameraOpaqueCache, aCameraTransparentCache);
                
                //Render scene
                mpRenderSystem->BeginFrame(inpRenderTarget);
                
                //Perform the ambient pass
                mpRenderSystem->SetLight(pAmbientLight);
                SortOpaque(mpActiveCamera, aCameraOpaqueCache);
                Render(mpActiveCamera, ShaderPass::k_ambient, aCameraOpaqueCache);
                
                //Perform the diffuse pass
                if(aDirLightCache.empty() == false || aPointLightCache.empty() == false)
                {
                    mpRenderSystem->SetBlendFunction(AlphaBlend::k_one, AlphaBlend::k_one);
                    mpRenderSystem->LockBlendFunction();
                    
                    mpRenderSystem->EnableDepthWriting(false);
                    mpRenderSystem->LockDepthWriting();
                    
                    mpRenderSystem->EnableAlphaBlending(true);
                    mpRenderSystem->LockAlphaBlending();
                    
                    for(u32 i=0; i<aDirLightCache.size(); ++i)
                    {
                        mpRenderSystem->SetLight(aDirLightCache[i]);
                        Render(mpActiveCamera, ShaderPass::k_directional, aCameraOpaqueCache);
                    }
                    
                    for(u32 i=0; i<aPointLightCache.size(); ++i)
                    {
                        mpRenderSystem->SetLight(aPointLightCache[i]);
                        std::vector<RenderComponent*> aPointLightOpaqueCache;
                        CullRenderables(aPointLightCache[i], aCameraOpaqueCache, aPointLightOpaqueCache);
                        Render(mpActiveCamera, ShaderPass::k_point, aPointLightOpaqueCache);
                    }
                    
                    mpRenderSystem->UnlockAlphaBlending();
                    mpRenderSystem->UnlockDepthWriting();
                    mpRenderSystem->UnlockBlendFunction();
                }

                SortTransparent(mpActiveCamera, aCameraTransparentCache);
                Render(mpActiveCamera, ShaderPass::k_ambient, aCameraTransparentCache);
                
                mpRenderSystem->SetLight(nullptr);
                RenderUI(inpScene->GetWindow());
                
                //Present contents of buffer to screen
                if (inpRenderTarget != nullptr)
                {
                	inpRenderTarget->Discard();
                }
                mpRenderSystem->EndFrame(inpRenderTarget);
            }
            else
            {
                //Render the UI only
                //Clear the frame buffer ready for rendering
                mpRenderSystem->BeginFrame(inpRenderTarget);
                
                mpRenderSystem->SetLight(nullptr);
                RenderUI(inpScene->GetWindow());
                
                //Present contents of buffer to screen
                if (inpRenderTarget != nullptr)
                {
                	inpRenderTarget->Discard();
                }
                mpRenderSystem->EndFrame(inpRenderTarget);
            }
        }
        //----------------------------------------------------------
		/// Find Renderable Objects In Scene
		//----------------------------------------------------------
        void Renderer::FindRenderableObjectsInScene(Core::Scene* pScene, std::vector<RenderComponent*>& outaRenderCache, std::vector<CameraComponent*>& outaCameraCache,
                                          std::vector<DirectionalLightComponent*>& outaDirectionalLightComponentCache, std::vector<PointLightComponent*>& outaPointLightComponentCache, AmbientLightComponent*& outpAmbientLight) const
		{
            static std::vector<LightComponent*> aLightComponentCache;
            aLightComponentCache.clear();
            
            pScene->QuerySceneForComponents<RenderComponent, CameraComponent, LightComponent>(outaRenderCache, outaCameraCache, aLightComponentCache);
            
            //Split the lights
            for(u32 i=0; i<aLightComponentCache.size(); ++i)
            {
                if(aLightComponentCache[i]->GetEntity()->IsVisible() == false)
                    continue;
                
                if(aLightComponentCache[i]->IsA(DirectionalLightComponent::InterfaceID))
                {
                    outaDirectionalLightComponentCache.push_back((DirectionalLightComponent*)aLightComponentCache[i]);
                }
                else if(aLightComponentCache[i]->IsA(PointLightComponent::InterfaceID))
                {
                    outaPointLightComponentCache.push_back((PointLightComponent*)aLightComponentCache[i]);
                }
                else if(aLightComponentCache[i]->IsA(AmbientLightComponent::InterfaceID))
                {
                    outpAmbientLight = (AmbientLightComponent*)aLightComponentCache[i];
                }
            }
		}
        //----------------------------------------------------------
        /// Get Cull Predicate
        //----------------------------------------------------------
        ICullingPredicateSPtr Renderer::GetCullPredicate(CameraComponent* inpActiveCamera) const
        {
            ICullingPredicateSPtr pCullPredicate = inpActiveCamera->GetCullingPredicate();
            
            if(pCullPredicate)
            {
                return pCullPredicate;
            }
            if(inpActiveCamera->IsOrthographicView())
            {
                return mpOrthoCullPredicate;
            }
            
            return mpPerspectiveCullPredicate;
        }
        //----------------------------------------------------------
        /// Sort Opaque
        //----------------------------------------------------------
        void Renderer::SortOpaque(CameraComponent* inpCameraComponent, std::vector<RenderComponent*>& inaRenderables) const
        {
            RendererSortPredicateSPtr pOpaqueSort = inpCameraComponent->GetOpaqueSortPredicate();
            if(!pOpaqueSort)
            {
                pOpaqueSort = mpOpaqueSortPredicate;
            }
            
            if(pOpaqueSort)
            {
                pOpaqueSort->PrepareForSort(&inaRenderables);
				std::sort(inaRenderables.begin(), inaRenderables.end(), Core::MakeDelegate(pOpaqueSort.get(), &RendererSortPredicate::SortItem));
            }
        }
        //----------------------------------------------------------
        /// Sort Transparent
        //----------------------------------------------------------
        void Renderer::SortTransparent(CameraComponent* inpCameraComponent, std::vector<RenderComponent*>& inaRenderables) const
        {
            RendererSortPredicateSPtr pTransparentSort = inpCameraComponent->GetTransparentSortPredicate();
            if(!pTransparentSort)
            {
                pTransparentSort = mpTransparentSortPredicate;
            }
            
			if(pTransparentSort)
            {
				pTransparentSort->PrepareForSort(&inaRenderables);
				std::sort(inaRenderables.begin(), inaRenderables.end(), Core::MakeDelegate(pTransparentSort.get(), &RendererSortPredicate::SortItem));
			}
        }
        //----------------------------------------------------------
        /// Render Shadow Map
        //----------------------------------------------------------
        void Renderer::RenderShadowMap(CameraComponent* inpCameraComponent, std::vector<DirectionalLightComponent*>& inaLightComponents, std::vector<RenderComponent*>& inaRenderables)
        {
            std::vector<RenderComponent*> aFilteredShadowMapRenderCache;
            
            if(inaLightComponents.size() > 0)
            {
                //Cull items based on whether they cast shadows
                FilterShadowMapRenderables(inaRenderables, aFilteredShadowMapRenderCache);
            }
            
            for(u32 i=0; i<inaLightComponents.size(); ++i)
            {
                if(inaLightComponents[i]->GetShadowMapPtr() != nullptr)
                {
                    mpRenderSystem->SetLight(inaLightComponents[i]);
                    RenderShadowMap(mpActiveCamera, inaLightComponents[i], aFilteredShadowMapRenderCache);
                }
            }
        }
        //----------------------------------------------------------
		/// Render Shadow Map
		//----------------------------------------------------------
		void Renderer::RenderShadowMap(CameraComponent* inpCameraComponent, DirectionalLightComponent* inpLightComponent, std::vector<RenderComponent*>& inaRenderables)
		{
			//Create a new offscreen render target using the given texture
			RenderTarget* pRenderTarget = mpRenderSystem->CreateRenderTarget(inpLightComponent->GetShadowMapPtr()->GetWidth(), inpLightComponent->GetShadowMapPtr()->GetHeight());
			pRenderTarget->SetTargetTextures(inpLightComponent->GetShadowMapDebugPtr(), inpLightComponent->GetShadowMapPtr());
            
            mpRenderSystem->BeginFrame(pRenderTarget);
            
            //Only opaque objects cast and receive shadows
            for(std::vector<RenderComponent*>::const_iterator it = inaRenderables.begin(); it != inaRenderables.end(); ++it)
            {
                (*it)->RenderShadowMap(mpRenderSystem, inpCameraComponent);
            }
            
            mpRenderSystem->EndFrame(pRenderTarget);
            
            CS_SAFEDELETE(pRenderTarget);
		}
        //----------------------------------------------------------
		/// Render
		//----------------------------------------------------------
		void Renderer::Render(CameraComponent* inpCameraComponent, ShaderPass ineShaderPass, std::vector<RenderComponent*>& inaRenderables)
		{
            for(std::vector<RenderComponent*>::const_iterator it = inaRenderables.begin(); it != inaRenderables.end(); ++it)
            {
                (*it)->Render(mpRenderSystem, inpCameraComponent, ineShaderPass);
            }
			
            //The final dynamic sprite batch needs to be flushed
            mpRenderSystem->GetDynamicSpriteBatchPtr()->ForceRender(mpRenderSystem);
        }
        //----------------------------------------------------------
        /// Render UI
        //----------------------------------------------------------
        void Renderer::RenderUI(GUI::Window* inpWindow)
        {
            mpRenderSystem->ApplyCamera(Core::Vector3::ZERO, Core::Matrix4x4::IDENTITY, CreateOverlayProjection(inpWindow), Core::Colour::k_cornflowerBlue);
			mCanvas.Render(inpWindow, 1.0f);
        }
        //----------------------------------------------------------
        /// Cull Renderables
        //----------------------------------------------------------
		void Renderer::CullRenderables(CameraComponent* inpCamera, const std::vector<RenderComponent*>& inaRenderCache, std::vector<RenderComponent*>& outaRenderCache) const
		{
            ICullingPredicate * pCullingPredicate = GetCullPredicate(inpCamera).get();
            
            if(pCullingPredicate == nullptr)
            {
                outaRenderCache = inaRenderCache;
                return;
            }
            
            outaRenderCache.reserve(inaRenderCache.size());
            
            inpCamera->UpdateFrustum();
            
			for(std::vector<RenderComponent*>::const_iterator it = inaRenderCache.begin(); it != inaRenderCache.end(); ++it)
			{
				RenderComponent* pRenderable = (*it);
				
				if(pRenderable->IsVisible() == false)
                {
                    continue;
                }
                
                if(pRenderable->IsCullingEnabled() == false || pCullingPredicate->CullItem(inpCamera, pRenderable) == false)
                {
                    outaRenderCache.push_back(pRenderable);
                    continue;
                }
			}
		}
        //----------------------------------------------------------
        /// Cull Renderables
        //----------------------------------------------------------
		void Renderer::CullRenderables(PointLightComponent* inpLightComponent, const std::vector<RenderComponent*>& inaRenderCache, std::vector<RenderComponent*>& outaRenderCache) const
        {
            //Reserve estimated space
            outaRenderCache.reserve(inaRenderCache.size());
            
            Core::Sphere aLightSphere;
            aLightSphere.vOrigin = inpLightComponent->GetWorldPosition();
            aLightSphere.fRadius = inpLightComponent->GetRangeOfInfluence();
            
            for(std::vector<RenderComponent*>::const_iterator it = inaRenderCache.begin(); it != inaRenderCache.end(); ++it)
            {
                if(Core::ShapeIntersection::Intersects(aLightSphere, (*it)->GetBoundingSphere()) == true)
                {
                    outaRenderCache.push_back(*it);
                }
            }
        }
        //----------------------------------------------------------
        /// Filter Scene Renderables
        //----------------------------------------------------------
		void Renderer::FilterSceneRenderables(const std::vector<RenderComponent*>& inaRenderables, std::vector<RenderComponent*>& outaOpaque, std::vector<RenderComponent*>& outaTransparent) const
		{
            //Reserve estimated space
            outaOpaque.reserve(inaRenderables.size());
            outaTransparent.reserve(inaRenderables.size());
            
			for(std::vector<RenderComponent*>::const_iterator it = inaRenderables.begin(); it != inaRenderables.end(); ++it)
			{
				RenderComponent* pRenderable = (*it);
                pRenderable->IsTransparent() ? outaTransparent.push_back(pRenderable) : outaOpaque.push_back(pRenderable);
			}
		}
        //----------------------------------------------------------
        /// Filter Shadow Map Renderables
        //----------------------------------------------------------
        void Renderer::FilterShadowMapRenderables(const std::vector<RenderComponent*>& inaRenderables, std::vector<RenderComponent*>& outaRenderables) const
        {
            //Reserve estimated space
            outaRenderables.reserve(inaRenderables.size());
            
            for(std::vector<RenderComponent*>::const_iterator it = inaRenderables.begin(); it != inaRenderables.end(); ++it)
			{
				RenderComponent* pRenderable = (*it);
                if(pRenderable->IsShadowCastingEnabled() == true && pRenderable->IsTransparent() == false)
                {
                    outaRenderables.push_back(pRenderable);
                }
			}
        }
        //----------------------------------------------------------
        /// Create Overlay Projection
        //----------------------------------------------------------
        Core::Matrix4x4 Renderer::CreateOverlayProjection(GUI::Window* inpWindow) const
        {
            const Core::Vector2 kvOverlayDimensions(inpWindow->GetAbsoluteSize());
            const f32 kfOverlayNear = 1.0f;
            const f32 kfOverlayFar = 100.0f;
            return Core::Matrix4x4::CreateOrthoMatrixOffset(0, kvOverlayDimensions.x, 0, kvOverlayDimensions.y, kfOverlayNear, kfOverlayFar);
        }
	}
}

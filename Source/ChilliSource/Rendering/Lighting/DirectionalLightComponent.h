/*
 *  DirectionalLightComponent.h
 *  moFlo
 *
 *  Created by Scott Downie on 31/1/2014.
 *  Copyright 2014 Tag Games. All rights reserved.
 *
 */

#ifndef _MOFLOW_RENDERING_COMPONENTS_DIRECTIONALLIGHTCOMPONENT_H_
#define _MOFLOW_RENDERING_COMPONENTS_DIRECTIONALLIGHTCOMPONENT_H_

#include <ChilliSource/Rendering/Lighting/LightComponent.h>

namespace ChilliSource
{
	namespace Rendering
	{
		class CDirectionalLightComponent : public ILightComponent
		{
		public:
			DECLARE_NAMED_INTERFACE(CDirectionalLightComponent);
			
            //----------------------------------------------------------
            /// Constructor
            ///
            /// @param Shadow map target texture
            /// @param Shadow map debug target texture
            //----------------------------------------------------------
			CDirectionalLightComponent(const TexturePtr& inpShadowMapTarget, const TexturePtr& inpShadowMapDebugTarget = TexturePtr());
			//----------------------------------------------------------
			/// Is A
			///
			/// Returns if it is of the type given
			/// @param Comparison Type
			/// @return Whether the class matches the comparison type
			//----------------------------------------------------------
			bool IsA(Core::InterfaceIDType inInterfaceID) const;
            //----------------------------------------------------------
			/// Set Shadow Volume
			///
			/// @param Width
            /// @param Height
            /// @param Near
            /// @param Far
			//----------------------------------------------------------
			void SetShadowVolume(f32 infWidth, f32 infHeight, f32 infNear, f32 infFar);
            //----------------------------------------------------------
			/// Get Shadow Tolerance
			///
			/// @return Shadow tolerance factor
			//----------------------------------------------------------
			f32 GetShadowTolerance() const;
            //----------------------------------------------------------
			/// Set Shadow Tolerance
			///
			/// @param Shadow tolerance factor
			//----------------------------------------------------------
			void SetShadowTolerance(f32 infTolerance);
            //----------------------------------------------------------
            /// Get Direction
            ///
            /// @return Direction vector of light
            /// (only applies to directional lights)
            //----------------------------------------------------------
            const Core::CVector3& GetDirection() const;
            //----------------------------------------------------------
            /// Get Light Matrix
            ///
            /// @return Matrix to transform into light space
            //----------------------------------------------------------
            const Core::CMatrix4x4& GetLightMatrix() const;
            //----------------------------------------------------
			/// On Attached To Entity
			///
			/// Triggered when the component is attached to
			/// an entity
			//----------------------------------------------------
            void OnAttachedToEntity();
			//----------------------------------------------------
			/// On Detached From Entity
			///
			/// Triggered when the component is removed from
			/// an entity
			//----------------------------------------------------
            void OnDetachedFromEntity();
            //----------------------------------------------------
            /// On Entity Transform Changed
            ///
            /// Triggered when the entity transform changes
            /// and invalidates the light VP matrix
            //----------------------------------------------------
            void OnEntityTransformChanged();
            //----------------------------------------------------------
			/// Get Shadow Map Ptr
			///
			/// @return Shadow map texture
			//----------------------------------------------------------
			const TexturePtr& GetShadowMapPtr() const;
            //----------------------------------------------------------
			/// Get Shadow Map Debug Ptr
			///
			/// @return Shadow map debug colour texture
			//----------------------------------------------------------
			const TexturePtr& GetShadowMapDebugPtr() const;
            
        private:
            
            Core::CMatrix4x4 mmatProj;
            
            TexturePtr mpShadowMap;
            TexturePtr mpShadowMapDebug;
            
            mutable Core::CVector3 mvDirection;
            
            f32 mfShadowTolerance;
            
            mutable bool mbMatrixCacheValid;
		};
    }
}

#endif
//
//  StretchableImage.cpp
//  Chilli Source
//  Created by Scott Downie on 28/04/2011
//
//  The MIT License (MIT)
//
//  Copyright (c) 2011 Tag Games Limited
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

#include <ChilliSource/GUI/Image/StretchableImage.h>

#include <ChilliSource/Rendering/Texture/Texture.h>
#include <ChilliSource/Rendering/Texture/TextureAtlas.h>
#include <ChilliSource/Rendering/Base/CanvasRenderer.h>
#include <ChilliSource/Core/Base/Application.h>
#include <ChilliSource/Core/Base/Screen.h>
#include <ChilliSource/Core/String/StringParser.h>
#include <ChilliSource/Core/Resource/ResourcePool.h>

namespace ChilliSource
{
    namespace GUI
    {
		DEFINE_META_CLASS(StretchableImage)
		
		DEFINE_PROPERTY(TextureAtlas);
		DEFINE_PROPERTY(HeightMaintain);
		DEFINE_PROPERTY(WidthMaintain);
		DEFINE_PROPERTY(BaseTextureAtlasID);
		DEFINE_PROPERTY(CentreTouchConsumption);
		
		//--------------------------------------------------------
        /// Constructor
        /// 
        /// Empty
        //---------------------------------------------------------
        StretchableImage::StretchableImage()
		: HeightMaintain(false), WidthMaintain(false)
        {
            
        }
        //---------------------------------------------------------
        /// Constructor
        ///
        /// From param dictionary
        //---------------------------------------------------------
        StretchableImage::StretchableImage(const Core::ParamDictionary& insParams) 
		: GUIView(insParams), HeightMaintain(false), WidthMaintain(false)
        {
            std::string strValue;
            
            //---Texture
            Core::StorageLocation eTextureLocation = Core::StorageLocation::k_package;
            if(insParams.TryGetValue("TextureLocation", strValue))
            {
                eTextureLocation = CSCore::ParseStorageLocation(strValue);
            }
            if(insParams.TryGetValue("Texture", strValue))
            {
                Core::ResourcePool* resourcePool = Core::Application::Get()->GetResourcePool();
                SetTexture(resourcePool->LoadResource<Rendering::Texture>(eTextureLocation, strValue));
            }
            
            //---Sprite sheet
            Core::StorageLocation eTextureAtlasLocation = Core::StorageLocation::k_package;
            if(insParams.TryGetValue("TextureAtlasLocation", strValue))
            {
                eTextureAtlasLocation = CSCore::ParseStorageLocation(strValue);
            }
            if(insParams.TryGetValue("TextureAtlas", strValue))
            {
                Core::ResourcePool* resourcePool = Core::Application::Get()->GetResourcePool();
                SetTextureAtlas(resourcePool->LoadResource<Rendering::TextureAtlas>(eTextureAtlasLocation, strValue));
			}
			//---Sprite sheet base name
			if(insParams.TryGetValue("BaseTextureAtlasID", strValue))
            {
				SetBaseTextureAtlasID(strValue);
			}
			//---Maintain Width
			if(insParams.TryGetValue("HeightMaintain", strValue))
			{
				HeightMaintain = Core::ParseBool(strValue);
			}
			//---Maintain Height
			if(insParams.TryGetValue("WidthMaintain", strValue))
			{
				WidthMaintain = Core::ParseBool(strValue);
			}
			//---Set Maintain Width
			if(insParams.TryGetValue("SetHeightMaintain", strValue))
			{
				Core::Vector2 vSize = Core::ParseVector2(strValue);
				SetHeightMaintainingAspect(vSize.x, vSize.y);
			}
			//---Set Maintain Height
			if(insParams.TryGetValue("SetWidthMaintain", strValue))
			{
				Core::Vector2 vSize = Core::ParseVector2(strValue);
				SetWidthMaintainingAspect(vSize.x, vSize.y);
			}
        }
        //---------------------------------------------------------
        /// Set Texture
        ///
        /// @param Texture containing the nine patches
        //---------------------------------------------------------
        void StretchableImage::SetTexture(const Rendering::TextureCSPtr& inpTexture)
        {
            Texture = inpTexture;
        }
        //---------------------------------------------------------
        /// Get Texture
        ///
        /// @return Texture containing the nine patches
        //---------------------------------------------------------
        const Rendering::TextureCSPtr& StretchableImage::GetTexture() const
        {
            return Texture;
        }
        //---------------------------------------------------------
        /// Set Sprite Sheet
        ///
        /// @param Sprite sheet containing the nine patches
        //---------------------------------------------------------
        void StretchableImage::SetTextureAtlas(const Rendering::TextureAtlasCSPtr& inpTextureAtlas)
        {
            TextureAtlas = inpTextureAtlas;
        }
		//---------------------------------------------------------
		/// Get Sprite Sheet
		///
		/// @return Sprite sheet containing the nine patches
		//---------------------------------------------------------
		const Rendering::TextureAtlasCSPtr& StretchableImage::GetTextureAtlas() const
		{
			return TextureAtlas;
		}
		//---------------------------------------------------------
		//---------------------------------------------------------
		void StretchableImage::SetBaseTextureAtlasID(const std::string& instrID)
		{
            CS_ASSERT(TextureAtlas != nullptr, "Must have texture atlas to set IDs");
                
            BaseTextureAtlasID = instrID;
            
            std::string atlasId = instrID + "TopLeft";
            m_panels.m_topLeftSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_topLeftUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "TopCentre";
            m_panels.m_topCentreSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_topCentreUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "TopRight";
            m_panels.m_topRightSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_topRightUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "BottomLeft";
            m_panels.m_bottomLeftSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_bottomLeftUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "BottomCentre";
            m_panels.m_bottomCentreSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_bottomCentreUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "BottomRight";
            m_panels.m_bottomRightSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_bottomRightUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "MiddleLeft";
            m_panels.m_leftCentreSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_leftCentreUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "MiddleCentre";
            m_panels.m_middleCentreSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_middleCentreUVs = TextureAtlas->GetFrameUVs(atlasId);
            
            atlasId = instrID + "MiddleRight";
            m_panels.m_rightCentreSize = TextureAtlas->GetCroppedFrameSize(atlasId);
            m_panels.m_rightCentreUVs = TextureAtlas->GetFrameUVs(atlasId);
		}
		//---------------------------------------------------------
		//---------------------------------------------------------
		const std::string& StretchableImage::GetBaseTextureAtlasID() const
		{
			return BaseTextureAtlasID;
		}
        //---------------------------------------------------------
        /// Draw
        /// 
        /// Draw the image constructed from the nine patches
        ///
        /// @param Canvas renderer pointer
        //---------------------------------------------------------
        void StretchableImage::Draw(Rendering::CanvasRenderer* inpCanvas)
        {
			//Check if this is on screen
			Core::Vector2 vTopRight = GetAbsoluteScreenSpaceAnchorPoint(Rendering::AlignmentAnchor::k_topRight);
			Core::Vector2 vBottomLeft = GetAbsoluteScreenSpaceAnchorPoint(Rendering::AlignmentAnchor::k_bottomLeft);
			
			if(vTopRight.y < 0 || vBottomLeft.y > GetScreen()->GetResolution().y || vTopRight.x < 0 || vBottomLeft.x > GetScreen()->GetResolution().x)
			{
				//Offscreen
				return;
			}
			
            if(Visible && TextureAtlas && Texture)
            {
                Core::Vector2 vPanelPos = GetAbsoluteScreenSpacePosition();
                Core::Vector2 vTopLeft = GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_topLeft);
                Core::Vector2 vPatchPos;
                
                Core::Colour AbsColour = GetAbsoluteColour();
                
                //We need to use a matrix so that we can rotate all the patches with respect
                //to the view
                Core::Matrix3 matViewTransform = Core::Matrix3::CreateTransform(vPanelPos, Core::Vector2(1, 1), GetAbsoluteRotation());
				
				// Retrieve each bit's size
				PatchSize sPatchSize;
				CalculatePatchSize(sPatchSize);
                
                // Render ourself
				
                // Draw the top left corner
				Core::Matrix3 matPatchTransform = Core::Matrix3::CreateTranslation(vTopLeft);
				Core::Matrix3 matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform,
                                   sPatchSize.vSizeTopLeft,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_topLeftUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topLeft);
                
                // Draw the top right corner
				matPatchTransform = Core::Matrix3::CreateTranslation(GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_topRight));
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeTopRight,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_topRightUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topRight);
                
                // Draw the bottom left corner
				matPatchTransform = Core::Matrix3::CreateTranslation(GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_bottomLeft));
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeBottomLeft,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_bottomLeftUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_bottomLeft);
                
                // Draw the bottom right corner
				matPatchTransform = Core::Matrix3::CreateTranslation(GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_bottomRight));
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeBottomRight,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_bottomRightUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_bottomRight);
                
                // Draw the top
				vPatchPos.x = vTopLeft.x + sPatchSize.vSizeTopLeft.x;
				vPatchPos.y = GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_topCentre).y;
				matPatchTransform = Core::Matrix3::CreateTranslation(vPatchPos);
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeTopCentre,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_topCentreUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topLeft);
				
                // Draw the bottom
				vPatchPos.x = vTopLeft.x + sPatchSize.vSizeBottomLeft.x;
				vPatchPos.y = GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_bottomCentre).y;
				matPatchTransform = Core::Matrix3::CreateTranslation(vPatchPos);
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeBottomCentre,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_bottomCentreUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_bottomLeft);
                
                // Draw the left
				vPatchPos.x = GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_middleLeft).x;
				vPatchPos.y = vTopLeft.y - sPatchSize.vSizeTopLeft.y;
				matPatchTransform = Core::Matrix3::CreateTranslation(vPatchPos);
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeLeftCentre,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_leftCentreUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topLeft);
                
                // Draw the right
				vPatchPos.x = GetAbsoluteAnchorPoint(Rendering::AlignmentAnchor::k_middleRight).x;
				vPatchPos.y = vTopLeft.y - sPatchSize.vSizeTopRight.y;
				matPatchTransform = Core::Matrix3::CreateTranslation(vPatchPos);
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform, 
                                   sPatchSize.vSizeRightCentre,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_rightCentreUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topRight);
                
                // Draw the centre
				vPatchPos.x = vTopLeft.x + sPatchSize.vSizeTopLeft.x;
				vPatchPos.y = vTopLeft.y - sPatchSize.vSizeTopLeft.y;
				matPatchTransform = Core::Matrix3::CreateTranslation(vPatchPos);
				matTransform = matPatchTransform * matViewTransform;
                inpCanvas->DrawBox(matTransform,
                                   sPatchSize.vSizeMiddleCentre,
                                   Core::Vector2::k_zero,
								   Texture,
                                   m_panels.m_middleCentreUVs,
                                   AbsColour, 
                                   Rendering::AlignmentAnchor::k_topLeft);
                
                // Render subviews
                GUIView::Draw(inpCanvas);
            }
		}
		//--------------------------------------------------------
		/// Set Width Maintaining Aspect
		///
		/// Change the width of the image and resize the height
		/// to maintain the aspect ratio
		///
		/// @param Unified width
		//--------------------------------------------------------
		void StretchableImage::SetWidthMaintainingAspect(f32 infRelWidth, f32 infAbsWidth)
		{
            Core::Vector2 vCurrentSize = GetAbsoluteSize();
			f32 fAspectRatio = vCurrentSize.y / vCurrentSize.x;
			SetSize(infRelWidth, 0.0f, infAbsWidth, 0.0f);
			
			f32 fScaleY = GetAbsoluteScale().y;
			if(fScaleY == 0.0f)
				return;
			
			vCurrentSize = GetAbsoluteSize();
            f32 fAbsHeight = (fAspectRatio * vCurrentSize.x) / fScaleY;
			SetSize(infRelWidth, 0.0f, infAbsWidth, fAbsHeight);
		}
		//--------------------------------------------------------
		/// Set Height Maintaining Aspect
		///
		/// Change the height of the image and resize the width
		/// to maintain the aspect ratio
		///
		/// @param Unified height
		//--------------------------------------------------------
		void StretchableImage::SetHeightMaintainingAspect(f32 infRelHeight, f32 infAbsHeight)
		{
            Core::Vector2 vCurrentSize = GetAbsoluteSize();
			f32 fAspectRatio = vCurrentSize.x / vCurrentSize.y;
			SetSize(0.0f, infRelHeight, 0.0f, infAbsHeight);
			
			f32 fScaleX = GetAbsoluteScale().x;
			if(fScaleX == 0.0f)
				return;
			
			vCurrentSize = GetAbsoluteSize();
            f32 fAbsWidth = (fAspectRatio * vCurrentSize.y) / fScaleX;
			SetSize(0.0f, infRelHeight, fAbsWidth, infAbsHeight);
		}
		//--------------------------------------------------------
		/// Enable Height Maintaining Aspect
		///
		/// Enables auto scaling of the height to maintain the aspect ratio
		///
		/// @param boolean to disable or enable
		//--------------------------------------------------------
		void StretchableImage::EnableHeightMaintainingAspect(bool inbEnabled)
		{
			HeightMaintain = inbEnabled;
		}
		//--------------------------------------------------------
		/// Enable Width Maintaining Aspect
		///
		/// Enables auto scaling of the height to maintain the aspect ratio
		///
		/// @param boolean to disable or enable
		//--------------------------------------------------------
		void StretchableImage::EnableWidthMaintainingAspect(bool inbEnabled)
		{
			WidthMaintain = inbEnabled;            
		}
		//--------------------------------------------------------
		/// Is Width Maintaining Aspect Enabled
		///
		/// @return auto scaling of the Width to maintain the aspect ratio
		//--------------------------------------------------------
		bool StretchableImage::IsWidthMaintainingAspectEnabled() const
		{
			return WidthMaintain;
		}
		//--------------------------------------------------------
		/// Is Height Maintaining Aspect Enabled
		///
		/// @return auto scaling of the height to maintain the aspect ratio
		//--------------------------------------------------------
		bool StretchableImage::IsHeightMaintainingAspectEnabled() const
		{
			return HeightMaintain;
		}
		
		void StretchableImage::CalculatePatchSize(PatchSize& outPatchSize)
		{
			Core::Vector2 vPanelSize = GetAbsoluteSize();

			//Get the patch sizes
			outPatchSize.vSizeTopLeft = m_panels.m_topLeftSize;
			outPatchSize.vSizeTopRight = m_panels.m_topRightSize;
			outPatchSize.vSizeBottomLeft = m_panels.m_bottomLeftSize;
			outPatchSize.vSizeBottomRight = m_panels.m_bottomRightSize;
			
			//Check to see if they are going to fit in the bounds of the view
			f32 fTotal = outPatchSize.vSizeTopLeft.y + outPatchSize.vSizeBottomLeft.y;
			if(fTotal > vPanelSize.y)
			{
				//The corners are too tall let's
				//squish them
				f32 fScale = vPanelSize.y/fTotal;
				outPatchSize.vSizeTopLeft.y *= fScale;
				outPatchSize.vSizeBottomLeft.y *= fScale;
			}
			fTotal = outPatchSize.vSizeTopRight.y + outPatchSize.vSizeBottomRight.y;
			if(fTotal > vPanelSize.y)
			{
				//The corners are too tall let's
				//squish them
				f32 fScale = vPanelSize.y/fTotal;
				outPatchSize.vSizeTopRight.y *= fScale;
				outPatchSize.vSizeBottomRight.y *= fScale;
			}
			fTotal = outPatchSize.vSizeTopLeft.x + outPatchSize.vSizeTopRight.x;
			if(fTotal > vPanelSize.x)
			{
				//The corners are too tall let's
				//squish them
				f32 fScale = vPanelSize.x/fTotal;
				outPatchSize.vSizeTopLeft.x *= fScale;
				outPatchSize.vSizeTopRight.x *= fScale;
			}
			fTotal = outPatchSize.vSizeBottomLeft.x + outPatchSize.vSizeBottomRight.x;
			if(fTotal > vPanelSize.x)
			{
				//The corners are too tall let's
				//squish them
				f32 fScale = vPanelSize.x/fTotal;
				outPatchSize.vSizeBottomLeft.x *= fScale;
				outPatchSize.vSizeBottomRight.x *= fScale;
			}
			
			// Calculate the top
			outPatchSize.vSizeTopCentre.x = vPanelSize.x - (outPatchSize.vSizeTopLeft.x + outPatchSize.vSizeTopRight.x);
			outPatchSize.vSizeTopCentre.y = outPatchSize.vSizeTopLeft.y;
			
            // Calculate the bottom
			outPatchSize.vSizeBottomCentre.x = vPanelSize.x - (outPatchSize.vSizeBottomLeft.x + outPatchSize.vSizeBottomRight.x);
			outPatchSize.vSizeBottomCentre.y = outPatchSize.vSizeBottomLeft.y;
			
			// Calculate the left
			outPatchSize.vSizeLeftCentre.y = vPanelSize.y - (outPatchSize.vSizeTopLeft.y + outPatchSize.vSizeBottomLeft.y);
			outPatchSize.vSizeLeftCentre.x = outPatchSize.vSizeTopLeft.x;
			
			// Calculate the right
			outPatchSize.vSizeRightCentre.y = vPanelSize.y - (outPatchSize.vSizeTopRight.y + outPatchSize.vSizeBottomRight.y);
			outPatchSize.vSizeRightCentre.x = outPatchSize.vSizeTopRight.x;
			
			// Calculate the centre
			outPatchSize.vSizeMiddleCentre.x = vPanelSize.x - (outPatchSize.vSizeLeftCentre.x + outPatchSize.vSizeRightCentre.x);
			outPatchSize.vSizeMiddleCentre.y = vPanelSize.y - (outPatchSize.vSizeTopCentre.y + outPatchSize.vSizeBottomCentre.y);
		}
    }
}

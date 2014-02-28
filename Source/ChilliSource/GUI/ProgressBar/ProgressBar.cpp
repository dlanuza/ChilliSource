//
//  GUIProgressBar.cpp
//  moFlo
//
//  Created by Scott Downie on 27/04/2011.
//  Copyright 2011 Tag Games. All rights reserved.
//

#include <ChilliSource/GUI/ProgressBar/ProgressBar.h>

#include <ChilliSource/Core/Math/MathUtils.h>
#include <ChilliSource/Core/Base/Application.h>
#include <ChilliSource/Core/String/StringParser.h>

namespace ChilliSource
{
    namespace GUI
    {
		DEFINE_META_CLASS(ProgressBar)
        
        
        ProgressBar::ProgressBar()
		:mfProgress(0.0f)
		,mfAnimationTime(0.0f)
		,mfPreviousValue(0)
		,mPreviousValueTimeStamp(0)
		,mfPreviousValueDiff(0)
        {
            
        }
        
        ProgressBar::ProgressBar(const Core::ParamDictionary& insParams)
		:GUIView(insParams)
		,mfProgress(0.0f)
		,mfAnimationTime(0.0f)
		,mfPreviousValue(0)
		,mPreviousValueTimeStamp(0)
		,mfPreviousValueDiff(0)
        {
            std::string strValue;
            
            //---Animation
            if(insParams.TryGetValue("AnimationTime", strValue))
            {
                SetAnimatedTimeInSeconds(Core::ParseF32(strValue));
                SetProgress(mfProgress);
            }
        }
        //------------------------------------------------
        /// Set Progress
        ///
        /// Normalised progress value representing how
        /// far the bar will fill
        ///
        /// @param Value between 0 and 1
        //------------------------------------------------
        void ProgressBar::SetProgress(f32 infProgress)
        {            
            mfPreviousValue = GetProgress();
            mfProgress = Core::MathUtils::Clamp(infProgress, 0.0f, 1.0f);
			
			// Going backward: jump to it
			if(mfProgress < mfPreviousValue)
			{
				mfPreviousValue = mfProgress;
			}
			// Animate
			mfPreviousValueDiff = mfProgress - mfPreviousValue;
			mPreviousValueTimeStamp = Core::Application::GetSystemTimeInMilliseconds();
        }
        //------------------------------------------------
        /// Get Progress
        ///
        /// Normalised progress value representing how
        /// far the bar will fill
        ///
        /// If AnimationTime is not 0 the value returned will 
        /// change depending on the progress of the animation
        ///
        /// @return Value between 0 and 1
        //------------------------------------------------
        f32 ProgressBar::GetProgress() const
        {
            if(mfAnimationTime != 0)
            {
                f32 fTimeSinceValueSet = Core::Application::GetSystemTimeInMilliseconds() - mPreviousValueTimeStamp;
                f32 fTimeRatio = ChilliSource::Core::MathUtils::Min(fTimeSinceValueSet / mfAnimationTime, 1.0f);
                f32 fAnimatedProgress = Core::MathUtils::Clamp(mfPreviousValue + (mfPreviousValueDiff * fTimeRatio), 0.0f, 1.0f);
                return fAnimatedProgress;
                
            }
            return mfProgress;
        }
        //------------------------------------------------------------------------
        /// Set the time to animate between previous values and the new value
        ///
        /// @param Number of seconds the animation will run for
        //------------------------------------------------------------------------
        void ProgressBar::SetAnimatedTimeInSeconds(f32 infAnimationTime)
        {
            mfAnimationTime = infAnimationTime * 1000;
        }
        //------------------------------------------------------------------------
        /// Get the time of the animation between previous values and the new value
        ///
        /// @param Number of seconds the animation will run for
        //------------------------------------------------------------------------
        f32 ProgressBar::GetAnimationTime() 
        { 
            return mfAnimationTime;
        }
        
        void ProgressBar::Draw(Rendering::CanvasRenderer* inpCanvas)
        {
            GUIView::Draw(inpCanvas);
        }
    }
}
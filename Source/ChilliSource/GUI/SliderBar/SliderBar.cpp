//
//  Slider.cpp
//  moFloTest
//
//  Created by Scott Downie on 09/06/2011.
//  Copyright 2011 Tag Games. All rights reserved.
//

#include <ChilliSource/GUI/SliderBar/SliderBar.h>

#include <ChilliSource/Core/Math/MathUtils.h>

namespace ChilliSource
{
    namespace GUI
    {
		DEFINE_META_CLASS(SliderBar)

        //----------------------------------------------
        /// Constructor
        ///
        /// Default
        //----------------------------------------------
        SliderBar::SliderBar() : mfSliderValue(0.0f)
        {
            
        }
        //----------------------------------------------
        /// Constructor
        ///
        /// From Params
        //----------------------------------------------
        SliderBar::SliderBar(const Core::ParamDictionary& insParams) : GUIView(insParams), mfSliderValue(0.0f)
        {
            
        }
        //----------------------------------------------
        /// Get Value
        ///
        /// @return Normalised value of the slider pos
        //----------------------------------------------
        f32 SliderBar::GetValue() const
        {
            return Core::MathUtils::Clamp(mfSliderValue, 0.0f, 1.0f);
        }
        //----------------------------------------------
        /// Set Value
        ///
        /// @param Normalised value of the slider pos
        //----------------------------------------------
        void SliderBar::SetValue(f32 infValue)
        {
            mfSliderValue=Core::MathUtils::Clamp(infValue, 0.0f, 1.0f);
        }		
		
    }
}
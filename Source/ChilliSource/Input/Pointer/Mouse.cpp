
#include <ChilliSource/Input/Pointer/Mouse.h>

namespace ChilliSource
{
	namespace Input
	{
		DEFINE_NAMED_INTERFACE(IMouse);

		//------------------------------------------------------
		/// Constructor
		///
		/// Default
		//------------------------------------------------------
		IMouse::IMouse() : mpTouchProxy(NULL)
		{
			memset(mbaButtonsDown, false, sizeof(bool) * (u32)MouseInputType::k_total);
		}
		//------------------------------------------------------
		/// Constructor
		///
		/// Takes a touch screen proxy to fake touch input
		///
		/// @param Touch screen proxy
		//------------------------------------------------------
		IMouse::IMouse(ITouchScreen* inpTouchProxy) : mpTouchProxy(inpTouchProxy)
		{
			memset(mbaButtonsDown, false, sizeof(bool) * (u32)MouseInputType::k_total);
		}
		//------------------------------------------------------
		/// Is Button Down
		///
		/// @param Mouse button type
		/// @return Whether the mouse button is down
		//------------------------------------------------------
		bool IMouse::IsButtonDown(MouseInputType ineButton) const
		{
			return mbaButtonsDown[(u32)ineButton];
		}
		//------------------------------------------------------
		/// Get Mouse Pressed Event
		///
		/// @return Event triggered on mouse button down
		//------------------------------------------------------
		Core::IEvent<MouseEventDelegate> & IMouse::GetMousePressedEvent()
		{
			return mOnMousePressedEvent;
		}
		//------------------------------------------------------
		/// Get Mouse Moved Event
		///
		/// @return Event triggered on mouse moved
		//------------------------------------------------------
		Core::IEvent<MouseEventDelegate> & IMouse::GetMouseMovedEvent()
		{
			return mOnMouseMovedEvent;
		}
		//------------------------------------------------------
		/// Get Mouse Released Event
		///
		/// @return Event triggered on mouse button up
		//------------------------------------------------------
		Core::IEvent<MouseEventDelegate> & IMouse::GetMouseReleasedEvent()
		{
			return mOnMouseReleasedEvent;
		}
	}
}
//
//  Video.h
//  CMMatchDay
//
//  Created by Scott Downie on 12/05/2011.
//  Copyright 2011 Tag Games. All rights reserved.
//

#ifndef _MO_FLO_VIDEO_VIDEO_PLAYER_H_
#define _MO_FLO_VIDEO_VIDEO_PLAYER_H_

#include <ChilliSource/ChilliSource.h>
#include <ChilliSource/Core/Base/Activity.h>
#include <ChilliSource/Core/Base/Colour.h>
#include <ChilliSource/Core/Event/Event.h>
#include <ChilliSource/Core/File/FileSystem.h>

namespace ChilliSource
{
    namespace Video
    {
		typedef std::function<void()> VideoDismissedEventDelegate;
		typedef std::function<void()> VideoPlaybackEventDelegate;
        
        class VideoPlayerActivity : public Core::Activity
        {
        public:
            CS_DECLARE_NAMEDTYPE(VideoPlayerActivity);
			
            //-------------------------------------------------------
            /// Create the platform dependent backend
            ///
            /// @author S Downie
            ///
            /// @return New backend instance
            //-------------------------------------------------------
            static VideoPlayerActivityUPtr Create();
            
            virtual ~VideoPlayerActivity(){}
            //--------------------------------------------------------------
            /// Present
            ///
            /// Begin streaming the video from file
            ///
            /// @param Storage location
            /// @param Video filename
            /// @param Whether or not the video can be dismissed by tapping.
            /// @param Background colour
            //--------------------------------------------------------------
            virtual void Present(Core::StorageLocation ineLocation, const std::string& instrFileName, bool inbCanDismissWithTap, const Core::Colour& inBackgroundColour = Core::Colour::k_black) = 0;
            //--------------------------------------------------------------
            /// Present With Subtitles
            ///
            /// Begin streaming the video from file with subtitles.
            ///
            /// @param Video Storage location
            /// @param Video filename
            /// @param Subtitles storage location.
            /// @param Subtitles filename.
            /// @param Whether or not the video can be dismissed by tapping.
            /// @param Background colour
            //--------------------------------------------------------------
            virtual void PresentWithSubtitles(Core::StorageLocation ineVideoLocation, const std::string& instrVideoFilename,
                                              Core::StorageLocation ineSubtitlesLocation, const std::string& instrSubtitlesFilename,
                                              bool inbCanDismissWithTap, const Core::Colour& inBackgroundColour = Core::Colour::k_black) = 0;
            //--------------------------------------------------------------
            /// Is Playing
            ///
            /// @return Whether a video is currently playing
            //--------------------------------------------------------------
            virtual bool IsPlaying() const = 0;
            //--------------------------------------------------------------
            /// Dismiss
            ///
            /// End playback of the currently playing video
            //--------------------------------------------------------------
            virtual void Dismiss() = 0;
            //--------------------------------------------------------------
            /// Get Duration
            ///
            /// @return The length of the video in seconds
            //--------------------------------------------------------------
            virtual f32 GetDuration() const = 0;
            //--------------------------------------------------------------
            /// Get Dismissed Event
            ///
            /// @return Event thats triggered when the video gets dismissed
            ///			by the player.
            //--------------------------------------------------------------
            Core::IConnectableEvent<VideoDismissedEventDelegate>& GetDismissedEvent();
            //--------------------------------------------------------------
            /// Get Playback Complete Event
            ///
            /// @return Event thats triggered when the video stops
            //--------------------------------------------------------------
            Core::IConnectableEvent<VideoPlaybackEventDelegate>& GetPlaybackCompleteEvent();
            //--------------------------------------------------------------
            /// Gets the current time of the video
            ///
            /// @return The elapsed time of the video
            //--------------------------------------------------------------
            virtual f32 GetTime() const = 0;
            
        protected:
            
            //-------------------------------------------------------
            /// Private constructor to force use of factory method
            ///
            /// @author S Downie
            //-------------------------------------------------------
            VideoPlayerActivity(){}
            
        protected:

            Core::Event<VideoDismissedEventDelegate> mOnDismissedEvent;
            Core::Event<VideoPlaybackEventDelegate> mOnPlaybackCompleteEvent;
        };
    }
}

#endif

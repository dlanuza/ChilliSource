/**
 * CFacebookPostSystem.cpp
 * moFlow
 *
 * Created by Robert Henning on 03/05/2012
 * Copyright �2012 Tag Games Limited - All rights reserved
 */

#include <ChilliSource/Backend/Platform/Android/Social/Facebook/FacebookJavaInterface.h>
#include <ChilliSource/Backend/Platform/Android/Social/Facebook/FacebookPostSystem.h>
#include <ChilliSource/Core/Base/MakeDelegate.h>

namespace ChilliSource
{
	using namespace ChilliSource::Social;

	namespace Android
	{
		FacebookPostSystem::FacebookPostSystem(Social::FacebookAuthenticationSystem* inpAuthSystem) : mpAuthSystem(inpAuthSystem)
		{
			mpJavaInterface = static_cast<FacebookAuthenticationSystem*>(mpAuthSystem)->GetJavaInterface();
			mpJavaInterface->SetPostSystem(this);
		}

		bool FacebookPostSystem::IsA(Core::InterfaceIDType inID) const
		{
			return Social::FacebookPostSystem::InterfaceID == inID || FacebookPostSystem::InterfaceID == inID;
		}

		void FacebookPostSystem::TryPost(const FacebookPostDesc& insDesc, const FacebookPostSystem::PostResultDelegate& insResultCallback)
		{
			mCompletionDelegate = insResultCallback;

            if(mpAuthSystem->IsSignedIn())
            {
                if(mpAuthSystem->HasPermission("publish_actions"))
                {
                    Post(insDesc);
                }
                else
                {
                    msPostDesc = insDesc;
                    std::vector<std::string> aWritePerms;
                    aWritePerms.push_back("publish_actions");
                    mpAuthSystem->AuthoriseWritePermissions(aWritePerms, Core::MakeDelegate(this, &FacebookPostSystem::OnPublishPermissionAuthorised));
                }
            }
            else
            {
                CS_LOG_ERROR("Facebook Post: User must be authenticated");
            }
		}

		void FacebookPostSystem::TrySendRequest(const Social::FacebookPostDesc& insDesc, const PostResultDelegate& insResultCallback, std::vector<std::string>& inastrRecommendedFriends)
		{
			mRequestCompleteDelegate = insResultCallback;

			if(mpAuthSystem->IsSignedIn())
			{
                if(mpAuthSystem->HasPermission("publish_actions"))
                {
                    PostRequest(insDesc);
                }
                else
                {
                    msPostDesc = insDesc;
                    std::vector<std::string> aWritePerms;
                    aWritePerms.push_back("publish_actions");
                    mpAuthSystem->AuthoriseWritePermissions(aWritePerms, Core::MakeDelegate(this, &FacebookPostSystem::OnPublishPermissionAuthorised));
                }
            }
            else
            {
                CS_LOG_ERROR("Facebook Post: User must be authenticated");
            }
		}

		void CreateKeyValueArrayFromPostDesc(const FacebookPostDesc& insDesc, std::vector<std::string>& outaKeyValues)
		{
			outaKeyValues.push_back("link");
			outaKeyValues.push_back(insDesc.strURL);

			outaKeyValues.push_back("picture");
			outaKeyValues.push_back(insDesc.strPictureURL);

			outaKeyValues.push_back("name");
			outaKeyValues.push_back(insDesc.strName);

			outaKeyValues.push_back("caption");
			outaKeyValues.push_back(insDesc.strCaption);

			outaKeyValues.push_back("description");
			outaKeyValues.push_back(insDesc.strDescription);
        }

		void CreateKeyValueArrayFromRequestPostDesc(const FacebookPostDesc& insDesc, std::vector<std::string>& outaKeyValues)
		{
			outaKeyValues.push_back("picture");
			outaKeyValues.push_back(insDesc.strPictureURL);

			outaKeyValues.push_back("name");
			outaKeyValues.push_back(insDesc.strName);

			outaKeyValues.push_back("caption");
			outaKeyValues.push_back(insDesc.strCaption);

			outaKeyValues.push_back("message");
			outaKeyValues.push_back(insDesc.strDescription);

			outaKeyValues.push_back("to");
			outaKeyValues.push_back(insDesc.strTo);
        }

		void FacebookPostSystem::Post(const FacebookPostDesc& insDesc)
		{
            std::string strGraphPath = "me/feed";
            if(!insDesc.strTo.empty())
            {
            	strGraphPath = insDesc.strTo + "/feed";
            }

            std::vector<std::string> aPostParamsKeyValue;
            CreateKeyValueArrayFromPostDesc(insDesc, aPostParamsKeyValue);
            mpJavaInterface->TryPostToFeed(strGraphPath, aPostParamsKeyValue);
		}

		void FacebookPostSystem::PostRequest(const Social::FacebookPostDesc& insDesc)
		{
            std::vector<std::string> aPostParamsKeyValue;
            CreateKeyValueArrayFromRequestPostDesc(insDesc, aPostParamsKeyValue);
            mpJavaInterface->TryPostRequest(aPostParamsKeyValue);
		}

		void FacebookPostSystem::OnPublishPermissionAuthorised(const Social::FacebookAuthenticationSystem::AuthenticateResponse& insResponse)
		{
            switch(insResponse.eResult)
            {
                case FacebookAuthenticationSystem::AuthenticateResult::k_success:
                    Post(msPostDesc);
                    break;
                case FacebookAuthenticationSystem::AuthenticateResult::k_permissionMismatch:
                    if(mCompletionDelegate)
                    {
                        mCompletionDelegate(Social::FacebookPostSystem::PostResult::k_cancelled);
                    }
                	break;
                case FacebookAuthenticationSystem::AuthenticateResult::k_failed:
                    if(mCompletionDelegate)
                    {
                        mCompletionDelegate(Social::FacebookPostSystem::PostResult::k_failed);
                    }
                    break;
            }
		}

		void FacebookPostSystem::OnPostToFeedComplete(bool inbSuccess)
		{
			if(!mCompletionDelegate)
			{
				return;
			}

			if(inbSuccess)
			{
				mCompletionDelegate(Social::FacebookPostSystem::PostResult::k_success);
			}
			else
			{
				mCompletionDelegate(Social::FacebookPostSystem::PostResult::k_failed);
            }
		}

		void FacebookPostSystem::OnPostRequestComplete(bool inbSuccess)
		{
			if(!mRequestCompleteDelegate)
			{
				return;
			}

			if(inbSuccess)
			{
				mRequestCompleteDelegate(Social::FacebookPostSystem::PostResult::k_success);
			}
			else
			{
				mRequestCompleteDelegate(Social::FacebookPostSystem::PostResult::k_failed);
            }
		}
	}
}

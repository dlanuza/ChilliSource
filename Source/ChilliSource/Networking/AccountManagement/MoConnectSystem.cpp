/*
 *  MoConnectSystem.cpp
 *  moFlow
 *
 *  Created by Stuart McGaw on 23/05/2011.
 *  Copyright 2011 Tag Games. All rights reserved.
 *
 */

#include <ChilliSource/Core/JSON/json.h>
#include <ChilliSource/Core/File/LocalDataStore.h>
#include <ChilliSource/Core/String/StringUtils.h>
#include <ChilliSource/Core/File/TweakableConstants.h>
#include <ChilliSource/Networking/AccountManagement/MoConnectSystem.h>

namespace ChilliSource
{
	using namespace Core;
	
	namespace Networking
    {
		DEFINE_NAMED_INTERFACE(CMoConnectSystem);
		
		const std::string CMoConnectSystem::kstrFacebookLoginType = "facebook";
		const std::string CMoConnectSystem::kstrEmailLoginType = "email";
        
        const std::string kstrPushNotificationAppleAPNS = "apns";
        const std::string kstrPushNotificationGoogleGCM = "gcm";
        const std::string kstrPushNotificationUnknown = "push-unknown";        
        const std::string kstrIAPApple = "apple";
        const std::string kstrIAPnGoogle = "google";
        const std::string kstrIAPUnknown = "iap-unknown";
#if DEBUG
        const std::string kstrEnvironment = "-dev";
#else
        const std::string kstrEnvironment = "";
#endif
        
        // Registers user keys
        const std::string kstrMoConnectIdKey = "MoConnectID";
        const std::string kstrMoConnectRealmKey = "MoConnectRLM";
        const std::string kstrMoConnectUserKey = "MoConnectName";
        const std::string kstrMoConnectOAuthTokenKey = "OAuthTokenKey";
        const std::string kstrMoConnectOAuthSecretKey = "OAuthTokenSecret";
        // Anonymous user keys
        const std::string kstrMoConnectAnonIdKey = "MoConnectAnonID";
        const std::string kstrMoConnectAnonRealmKey = "MoConnectAnonRLM";
        const std::string kstrMoConnectAnonOAuthTokenKey = "OAuthAnonTokenKey";
        const std::string kstrMoConnectAnonOAuthSecretKey = "OAuthAnonTokenSecret";
		
        //------------------------
        /// Constructor
        //------------------------
		CMoConnectSystem::CMoConnectSystem(IHttpConnectionSystem* inpHttpSystem, const std::string& instrMoConnectServerURL, Core::COAuthSystem* inpOAuthSystem)
		:mpHttpConnectionSystem(inpHttpSystem)
        ,mbHasSignedInUser(false)
        ,mstrMoConnectURL(instrMoConnectServerURL)
        ,mpPendingLoginsRequest(NULL)
        ,mudwRequestIDSeed(0)
        ,mbNoRemoveFulfilledRequests(false)
        ,mstrRealm(instrMoConnectServerURL)
        ,mpOAuthSystem(inpOAuthSystem)
		{
			
		}
        //------------------------
        /// Is A
        //------------------------
		bool CMoConnectSystem::IsA(Core::InterfaceIDType inInterfaceID) const
        {
			return inInterfaceID == CMoConnectSystem::InterfaceID;
		}
        //------------------------
        /// Get OAuth System
        //------------------------
        Core::COAuthSystem* CMoConnectSystem::GetOAuthSystem()
        {
            return mpOAuthSystem;
        }
		//------------------------
        /// Has Signed In User
        //------------------------
		bool CMoConnectSystem::HasSignedInUser() const
        {
			return mbHasSignedInUser;
		}
		//------------------------
        /// Get Current User ID
        //------------------------
		const std::string& CMoConnectSystem::GetCurrentUserID() const
        {
			if(HasSignedInUser())
            {
				return mstrUserID;
			}
            else
            {
				return Core::CStringUtils::BLANK;
			}
		}
        //------------------------
        /// Get Current User Name
        //------------------------
		const std::string& CMoConnectSystem::GetCurrentUserName() const
        {
			if(HasSignedInUser())
            {
				return mstrUserName;
			}
            else
            {
				return Core::CStringUtils::BLANK;
			}
		}
		//------------------------
        /// Set Current User Name
        //------------------------
		void CMoConnectSystem::SetCurrentUserName(const std::string& instrName)
        {
			mstrUserName = instrName;
			CLocalDataStore* pLDS = CLocalDataStore::GetSingletonPtr();
			pLDS->SetValueForKey(kstrMoConnectUserKey, mstrUserName); // User is inherently not going to be anon
			pLDS->Synchronise();
		}
        //------------------------
        /// Get Server Time
        //------------------------
		void CMoConnectSystem::GetServerTime(const CMoConnectSystem::ServerTimeDelegate& inDelegate)
		{
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrMoConnectURL + "/ping";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::TimeRequestCompletes));
			mTimeRequestCallback = inDelegate;
		}
        //------------------------
        /// Time Request Completes
        //------------------------
		void CMoConnectSystem::TimeRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
		{
			if(!mTimeRequestCallback)
				return;
			
			Json::Reader cReader;
			Json::Value cJResponse;
			if(ineResult == IHttpRequest::CompletionResult::k_completed && inpRequest->GetResponseCode() == kHTTPResponseOK)
			{
				if(cReader.parse(inpRequest->GetResponseString(), cJResponse))
				{
					if(cJResponse.isMember("Error"))
					{
						mTimeRequestCallback(0);
					}
					else
					{
						TimeIntervalSecs uddwResponse = (TimeIntervalSecs)cJResponse["Timestamp"].asUInt();
						mTimeRequestCallback(uddwResponse);
					}
				}
			}
			else
			{
				mTimeRequestCallback(0);
			}
			
			mTimeRequestCallback = NULL;
		}
        //------------------------
        /// Generate Authentication Header
        //------------------------
		void CMoConnectSystem::GenerateAuthenticationHeader(const std::string& instrURL, Core::ParamDictionary& outsHeader) const
        {
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, instrURL, "", strOAuthHeader);
            outsHeader.SetValueForKey("Authorization", strOAuthHeader);
            outsHeader.SetValueForKey("Content-Type", "application/json");
		}
		//------------------------
        /// Signed In User ChangesEvent
        //------------------------
		IEvent<CMoConnectSystem::EventDelegate>& CMoConnectSystem::SignedInUserChangesEvent()
        {
			return mSignedInUserChangesEvent;
		}
		//------------------------
        /// Create New Account
        //------------------------
		void CMoConnectSystem::CreateNewAccount(AccountCreateDelegate inDel)
        {
			SignOutCurrentUser();
			
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrMoConnectURL + "/user/create";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
            
            mpOAuthSystem->SetOAuthTokenKey("");
            mpOAuthSystem->SetOAuthTokenSecret("");
            
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            DEBUG_LOG(strOAuthHeader);
            
            mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
            mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::AccountCreateRequestCompletes));
			
			mAccountCreateCallback = inDel;
		}
		//------------------------
        /// Account Create Request Completes
        //------------------------
		void CMoConnectSystem::AccountCreateRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			AccountCreateResult eResult = AccountCreateResult::k_noServerResponse;
			
			if(IHttpRequest::CompletionResult::k_completed == ineResult && inpRequest->GetResponseCode() == kHTTPResponseOK)
            {
				Json::Reader cReader;
				Json::Value cJResponse;
				if(cReader.parse(inpRequest->GetResponseString(),cJResponse))
                {
					if(cJResponse.isMember("Error"))
                    {
						eResult = AccountCreateResult::k_serverRefuses;
					}
                    else
                    {
						mstrUserID = cJResponse["UserID"].asString();
						mstrRealm = "https://" + cJResponse["Realm"].asString();
						mbHasSignedInUser = true;
                        if(!cJResponse["Secret"].isNull())
                        {
                            // Response from server is not URL encoded
                            mstrOAuthTokenSecret = cJResponse["Secret"].asString();
                        }
                        if(!cJResponse["Token"].isNull())
                        {
                            // Response from server is not URL encoded
                            mstrOAuthToken = cJResponse["Token"].asString();
                        }
                        
                        mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
                        mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
                        
						OnUserChanged();
						eResult = AccountCreateResult::k_success;
					}
				}
			}
			
			if(mAccountCreateCallback)
            {
				mAccountCreateCallback(this, eResult);
			}
		}
        //------------------------
        /// Register Login Email
        //------------------------
        void CMoConnectSystem::RegisterLoginEmail(const std::string& instrID, const std::string& instrPassword, const RegisterLoginDelegate& inDel)
        {
            // Create Data
			Json::Value cCredentialsMsg(Json::objectValue);
            Json::Value jData(Json::objectValue);
			jData["Type"] = kstrEmailLoginType;
			jData["ID"] = instrID;
			jData["Password"] = instrPassword;
			cCredentialsMsg["Data"] = jData;
            
            // Register Login email
            RegisterLogin(cCredentialsMsg, inDel);
        }
        //------------------------
        /// Register Login Facebook
        //------------------------
        void CMoConnectSystem::RegisterLoginFacebook(const std::string& instrAccessToken, const RegisterLoginDelegate& inDel)
        {
            // Create Data
			Json::Value cCredentialsMsg(Json::objectValue);
            Json::Value jData(Json::objectValue);
			jData["Type"] = kstrFacebookLoginType;
			jData["AccessToken"] = instrAccessToken;
            
			cCredentialsMsg["Data"] = jData;
            
            // Register Login facebook
            RegisterLogin(cCredentialsMsg, inDel);
        }
        //------------------------
        /// Register Login
        //------------------------
		void CMoConnectSystem::RegisterLogin(const Json::Value& injData, RegisterLoginDelegate inDel)
        {
			if(HasSignedInUser())
            {
				HttpRequestDetails requestDetails;
				requestDetails.strURL = mstrRealm + "/login/register";
				requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
				
				Json::FastWriter cWriter;
				requestDetails.strBody = cWriter.write(injData);
                
                GenerateAuthenticationHeader(requestDetails.strURL, requestDetails.sHeaders);
				
				mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::RegisterLoginRequestCompletes));
                
                // Assume we're going to succeed, then undo it if we don't
				mastrCurrentAccountLogins.push_back(injData["Type"].asString());
				mRegisterLoginCallback = inDel;
			}
            else if(inDel)
            {
				inDel(this, RegisterLoginResult::k_authFailed);
			}
		}
        //------------------------
        /// Register Login Request Completes
        //------------------------
		void CMoConnectSystem::RegisterLoginRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			RegisterLoginResult eResult = RegisterLoginResult::k_noServerResponse;
			
			if(inpRequest->GetResponseCode() == kHTTPRedirect)
            {
				HandleRedirection(inpRequest);
				HttpRequestDetails sDetails = inpRequest->GetDetails();
				sDetails.strURL = mstrRealm + "/login/register";
				mpHttpConnectionSystem->MakeRequest(sDetails, inpRequest->GetCompletionDelegate());
				return;
			}
			
			if(ineResult == IHttpRequest::CompletionResult::k_completed)
            {
				DEBUG_LOG("RegisterLoginResponse:" + inpRequest->GetResponseString());
				Json::Reader cReader;
				Json::Value cJResponse;
                
				if(cReader.parse(inpRequest->GetResponseString(), cJResponse) && cJResponse.isMember("Error"))
                {
                    mastrCurrentAccountLogins.pop_back();
                    s32 dwResponse = cJResponse["Error"]["Code"].asInt();
                    
                    switch(RegisterLoginResult(dwResponse))
                    {
                        default:
                            break;
                        case RegisterLoginResult::k_authFailed:
                        case RegisterLoginResult::k_unknownCredentialType:
                        case RegisterLoginResult::k_credentialAlreadyUsed:
                        case RegisterLoginResult::k_invalidForm:
                        case RegisterLoginResult::k_invalidType:
                        case RegisterLoginResult::k_typeAlreadyUsed:
                            eResult = (RegisterLoginResult)dwResponse;
                            break;
                    }
				}
                else
                {
					eResult = RegisterLoginResult::k_success;
				}
			}
			
			if(mRegisterLoginCallback)
            {
				mRegisterLoginCallback(this, eResult);
			}
		}
        //------------------------
        /// Sign In Via Email
        //------------------------
        void CMoConnectSystem::SignInViaEmail(const std::string& instrID, const std::string& instrPassword, const SignInDelegate& inDel, bool inbGetAccountsOnly)
        {
            // Create Data
			Json::Value cCredentialsMsg(Json::objectValue);
            Json::Value jData(Json::objectValue);
			jData["Type"] = kstrEmailLoginType;
			jData["ID"] = instrID;
			jData["Password"] = instrPassword;
			cCredentialsMsg["Data"] = jData;
            
            // Sign in with email
            SignIn(cCredentialsMsg, inDel, inbGetAccountsOnly);
        }
        //------------------------
        /// Sign In Via Facebook
        //------------------------
        void CMoConnectSystem::SignInViaFacebook(const std::string& instrAccessToken, const SignInDelegate& inDel, bool inbGetAccountsOnly)
        {
            // Create Data
			Json::Value cCredentialsMsg(Json::objectValue);
            Json::Value jData(Json::objectValue);
			jData["Type"] = kstrFacebookLoginType;
			jData["AccessToken"] = instrAccessToken;
            
            if(inbGetAccountsOnly)
            {
                Json::Value jOptions(Json::objectValue);
                jOptions["CreateTokens"] = false;
                jOptions["AllowLifecycle"] = false;
                cCredentialsMsg["Options"] = jOptions;
            }
            
			cCredentialsMsg["Data"] = jData;
            
            // Sign in with facebook
            SignIn(cCredentialsMsg, inDel, inbGetAccountsOnly);
        }
        //------------------------
        /// Sign In
        //------------------------
        void CMoConnectSystem::SignIn(const Json::Value& injData, const SignInDelegate& inDel, bool inbRetrieveAccountsOnly)
        {
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrMoConnectURL + "/user/login";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
            
            mpOAuthSystem->SetOAuthTokenKey("");
            mpOAuthSystem->SetOAuthTokenSecret("");
            
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            
            mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
            mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			Json::FastWriter cWriter;
			requestDetails.strBody = cWriter.write(injData);
            
            // Retrieve accounts only
            if(inbRetrieveAccountsOnly)
            {
                mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::RetrieveAccountsRequestCompletes));
                mRetrieveAccountsCallback = inDel;
            }
            // Actual sign in request
            else
            {
                mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::SignInRequestCompletes));
                mSignInCallback = inDel;
            }
		}
        //------------------------
        /// Sign In Request Completes
        //------------------------
		void CMoConnectSystem::SignInRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			SignInResult eResult = SignInResult::k_noServerResponse;
            DYNAMIC_ARRAY<SignedInUser> asUsers;
            
			if(ineResult == IHttpRequest::CompletionResult::k_completed)
            {
				Json::Reader cReader;
				Json::Value cJResponse;
				if(cReader.parse(inpRequest->GetResponseString(), cJResponse))
                {
					if(cJResponse.isMember("Error"))
                    {
						s32 dwResponse = cJResponse["Error"]["Code"].asInt();
						eResult = (SignInResult) dwResponse;
					}
                    else
                    {
                        SignedInUser sUser;
						sUser.strUserID = mstrUserID = cJResponse["UserID"].asString();
						sUser.strRealm = mstrRealm = "https://" + cJResponse["Realm"].asString();
						sUser.strUserName = mstrUserName = cJResponse["Username"].asString();
						mbHasSignedInUser = true;
                        
                        if(!cJResponse["Secret"].isNull())
                        {
                            // Response from server is not URL encoded
                            sUser.strTokenSecret = mstrOAuthTokenSecret = cJResponse["Secret"].asString();
                        }
                        if(!cJResponse["Token"].isNull())
                        {
                            // Response from server is not URL encoded
                            sUser.strToken = mstrOAuthToken = cJResponse["Token"].asString();
						}
                        
                        mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
                        mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
                        
						eResult = SignInResult::k_success;
						OnUserChanged();
                        
                        asUsers.push_back(sUser);
					}
				}
			}
			
			if(mSignInCallback)
            {
				mSignInCallback(this, eResult, asUsers);
			}
		}
        //------------------------
        /// Try Sign In Request Completes
        //------------------------
		void CMoConnectSystem::RetrieveAccountsRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			SignInResult eResult = SignInResult::k_noServerResponse;
            DYNAMIC_ARRAY<SignedInUser> asUsers;
			
			if(ineResult == IHttpRequest::CompletionResult::k_completed)
            {
				Json::Reader cReader;
				Json::Value cJResponse;
				if(cReader.parse(inpRequest->GetResponseString(), cJResponse))
                {
					if(cJResponse.isMember("Error"))
                    {
						s32 dwResponse = cJResponse["Error"]["Code"].asInt();
						eResult = (SignInResult) dwResponse;
					}
                    else
                    {
						eResult = SignInResult::k_success;
                        
                        SignedInUser sUser;
						sUser.strUserID = cJResponse["UserID"].asString();
						sUser.strRealm = "https://" + cJResponse["Realm"].asString();
						sUser.strUserName = cJResponse["Username"].asString();
                        sUser.strToken = cJResponse["Token"].asString();
                        sUser.strTokenSecret = cJResponse["Secret"].asString();
                        asUsers.push_back(sUser);
					}
				}
			}
			
			if(mRetrieveAccountsCallback)
            {
				mRetrieveAccountsCallback(this, eResult, asUsers);
			}
		}
		//------------------------
        /// Logins Request Completes
        //------------------------
		void CMoConnectSystem::LoginsRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			mpPendingLoginsRequest = NULL;
			
			if(inpRequest->GetResponseCode() == kHTTPRedirect)
            {
				HandleRedirection(inpRequest);
				HttpRequestDetails sDetails = inpRequest->GetDetails();
				sDetails.strURL = mstrRealm + "/logins";
				mpPendingLoginsRequest = mpHttpConnectionSystem->MakeRequest(sDetails, inpRequest->GetCompletionDelegate());
				return;
			}
			
			if(ineResult == IHttpRequest::CompletionResult::k_completed)
            {
				Json::Reader cReader;
				Json::Value cJResponse;
				if(cReader.parse(inpRequest->GetResponseString(),cJResponse))
                {
					if(cJResponse.type() == Json::objectValue && cJResponse.isMember("Error"))
                    {
                        // Nothing to see here
					}
                    else
                    {
						if(!cJResponse.isArray())
                        {
							return;
                        }
                        
						for(u32 udwLogin = 0; udwLogin < cJResponse.size(); udwLogin++)
                        {
							mastrCurrentAccountLogins.push_back(cJResponse[udwLogin]["Type"].asString());
						}
					}
				}
			}
		}
        //------------------------
        /// Register For Push Notifications
        //------------------------
        void CMoConnectSystem::RegisterForPushNotifications(const PushNotificationType ineType, const std::string& instrToken,
                                                            const std::string& instrLanguage, const std::string& instrCountryCode,
                                                            const PushNotificationResultDelegate& inDelegate)
        {
            HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrRealm + "/push/register";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			
			Json::Value cRegistrationDetails(Json::objectValue);
			cRegistrationDetails["Service"]     = GetPushNotificationTypeAsString(ineType) + kstrEnvironment;
			cRegistrationDetails["DeviceToken"] = instrToken;
			cRegistrationDetails["Language"]    = instrLanguage;
            cRegistrationDetails["Locale"]      = instrCountryCode;
            
            Json::Value cRegistrationMsg(Json::objectValue);
			cRegistrationMsg["Data"] = cRegistrationDetails;
            
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            DEBUG_LOG(strOAuthHeader);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			Json::FastWriter cWriter;
			requestDetails.strBody = cWriter.write(cRegistrationMsg);
			DEBUG_LOG("RegisterForPushNotifications:"+requestDetails.strBody);
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::PushNotificationRequestCompletes));
            
            mPushNotificationCallback = inDelegate;
        }
        //------------------------
        /// Get Push Notification Type As String
        //------------------------
        const std::string& CMoConnectSystem::GetPushNotificationTypeAsString(const PushNotificationType ineType)
        {
            switch(ineType)
            {
                case PushNotificationType::k_appleAPNS:
                    return kstrPushNotificationAppleAPNS;
                case PushNotificationType::k_googleGCM:
                    return kstrPushNotificationGoogleGCM;
                    default:
                    ERROR_LOG("Unsupported push notification type!");
                    break;
            }
            
            return kstrPushNotificationUnknown;
        }
        //------------------------
        /// Push Notification Request Completes
        //------------------------
        void CMoConnectSystem::PushNotificationRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
            PushNotificationResult eResult = PushNotificationResult::k_success;
            
            if(ineResult != IHttpRequest::CompletionResult::k_completed)
            {
                if(ineResult == IHttpRequest::CompletionResult::k_failed)
                {
                    ERROR_LOG("Push notification registration failed!");
                }
                else if(ineResult == IHttpRequest::CompletionResult::k_cancelled)
                {
                    ERROR_LOG("Push notification registration was cancelled.");
                }
                else if(ineResult == IHttpRequest::CompletionResult::k_timeout)
                {
                    ERROR_LOG("Push notification registration timed out.");
                }
                else if(ineResult == IHttpRequest::CompletionResult::k_flushed)
                {
                    ERROR_LOG("Push notification registration buffer need to be flushed.");
                }
                
                eResult = PushNotificationResult::k_failed;
            }
            
            if(NULL != mPushNotificationCallback)
            {
                mPushNotificationCallback(eResult);
            }
        }
		//------------------------
        /// Request Local User Profile
        //------------------------
        void CMoConnectSystem::RequestLocalUserProfile(const LocalUserProfileDelegate& ineDelegate)
        {
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrRealm + "/me";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			
			std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::OnLocalUserProfileReceived));
			
			mLocalUserProfileDelegate = ineDelegate;
        }
		//------------------------
        /// On Me Received
        //------------------------
        void CMoConnectSystem::OnLocalUserProfileReceived(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
            Json::Value jResponse;
            switch (ineResult)
            {
                case IHttpRequest::CompletionResult::k_completed:
                {
                    Json::Reader cReader;
                    cReader.parse(inpRequest->GetResponseString(), jResponse);
                    break;
                }
                default:
                    break;
            }
            mLocalUserProfileDelegate(this, jResponse);
        }
		//------------------------
        /// Call Abandon
        //------------------------
        void CMoConnectSystem::RequestAccountAbandonment(const SignedInUser& insSignedUser)
        {
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrRealm + "/me/abandon";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
            
            // Ask to abandon a particular account
            if(!insSignedUser.strToken.empty())
            {
                requestDetails.strURL = insSignedUser.strRealm + "/me/abandon";
			    mpOAuthSystem->SetOAuthTokenKey(insSignedUser.strToken);
                mpOAuthSystem->SetOAuthTokenSecret(insSignedUser.strTokenSecret);
            }
            
			std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            
            // Put the token back
            mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
            mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			mpHttpConnectionSystem->MakeRequest(requestDetails, NULL);
        }
        //------------------------
        /// Handle Redirection
        //------------------------
		void CMoConnectSystem::HandleRedirection(HttpRequestPtr inpRequest)
        {
			Json::Reader cReader;
			Json::Value cResponse;
			if(cReader.parse(inpRequest->GetResponseString(), cResponse))
            {
				mstrRealm = "https://" + cResponse["Realm"].asString();
			}
		}
        //------------------------
        /// On User Changed
        //------------------------
		void CMoConnectSystem::OnUserChanged()
        {
			mbHasLoadedLoginTypes = false;
			mastrCurrentAccountLogins.clear();
			
			if (mpPendingLoginsRequest)
            {
				mpPendingLoginsRequest->Cancel();
				mpPendingLoginsRequest = NULL;
			}
            
			mSignedInUserChangesEvent.Invoke(this);
		}
		//------------------------
        /// Has Loaded LoginT ypes
        //------------------------
		bool CMoConnectSystem::HasLoadedLoginTypes()
        {
			return mbHasLoadedLoginTypes;
		}
        //------------------------
        /// Current Account Has Login
        //------------------------
		bool CMoConnectSystem::CurrentAccountHasLogin(const std::string& instrType)
        {
			return std::find(mastrCurrentAccountLogins.begin(), mastrCurrentAccountLogins.end(), instrType) != mastrCurrentAccountLogins.end();
		}
		//------------------------
        /// Try Restore User Details
        //------------------------
		bool CMoConnectSystem::TryRestoreUserDetails()
        {
			CLocalDataStore* pLDS = CLocalDataStore::GetSingletonPtr();
			pLDS->TryGetValue(kstrMoConnectUserKey, mstrUserName); //(not having a name is not at all blocking)
            
            std::string strTokenKey;
            std::string strTokenSecret;
            bool bSuccess = (pLDS->TryGetValue(kstrMoConnectIdKey, mstrUserID)
                          && pLDS->TryGetValue(kstrMoConnectRealmKey, mstrRealm)
                          && pLDS->TryGetValue(kstrMoConnectOAuthTokenKey, strTokenKey)
                          && pLDS->TryGetValue(kstrMoConnectOAuthSecretKey, strTokenSecret));
			if(!bSuccess)
			{
				//Try load any anonymous users
				bSuccess = (pLDS->TryGetValue(kstrMoConnectAnonIdKey, mstrUserID)
                         && pLDS->TryGetValue(kstrMoConnectAnonRealmKey, mstrRealm)
                         && pLDS->TryGetValue(kstrMoConnectAnonOAuthTokenKey, strTokenKey)
                         && pLDS->TryGetValue(kstrMoConnectAnonOAuthSecretKey, strTokenSecret));
				mbHasSignedInUser = bSuccess;
            }
            
            // Chances are we have saved a URL encoded OAuth token and secret
            // so we decode them here just in chase
            mstrOAuthToken = strTokenKey;
            mstrOAuthTokenSecret = strTokenSecret;
			mbHasSignedInUser = bSuccess;
            
            mpOAuthSystem->SetOAuthTokenKey(mstrOAuthToken);
            mpOAuthSystem->SetOAuthTokenSecret(mstrOAuthTokenSecret);
            
			OnUserChanged();
			
			return bSuccess;
		}
        //------------------------
        /// Save User Details
        //------------------------
		void CMoConnectSystem::SaveUserDetails(bool inbAnonymous)
		{
            std::string strMoConnectIdKey = "";
            std::string strMoConnectRealmKey = "";
            std::string strMoConnectUserNameKey = "";
            std::string strMoConnectOAuthTokenKey = "";
            std::string strMoConnectOAuthSecretKey = "";
            
            std::string strTokenKey = "";
            std::string strTokenSecret = "";
            mpOAuthSystem->GetOAuthTokenKey(strTokenKey);
            mpOAuthSystem->GetOAuthTokenSecret(strTokenSecret);
            
			if(inbAnonymous)
			{
                strMoConnectIdKey = kstrMoConnectAnonIdKey;
                strMoConnectRealmKey = kstrMoConnectAnonRealmKey;
                strMoConnectUserNameKey = "";
                strMoConnectOAuthTokenKey = kstrMoConnectAnonOAuthTokenKey;
                strMoConnectOAuthSecretKey = kstrMoConnectAnonOAuthSecretKey;
			}
			else
			{
                strMoConnectIdKey = kstrMoConnectIdKey;
                strMoConnectRealmKey = kstrMoConnectRealmKey;
                strMoConnectUserNameKey = kstrMoConnectUserKey;
                strMoConnectOAuthTokenKey = kstrMoConnectOAuthTokenKey;
                strMoConnectOAuthSecretKey = kstrMoConnectOAuthSecretKey;
            }
			
            CLocalDataStore* pLDS = CLocalDataStore::GetSingletonPtr();
            pLDS->SetValueForKey(strMoConnectIdKey, mstrUserID);
            pLDS->SetValueForKey(strMoConnectRealmKey, mstrRealm);
            if(!strMoConnectUserNameKey.empty())
            {
                pLDS->SetValueForKey(strMoConnectUserNameKey, mstrUserName);
            }
            // Values returned from OAuth System will be URL encoded. We must
            // decode before saving as the OAuth System set key/secret methods
            // automatically URL encode whatever string they are given - we don't
            // want to URL encode and already URL encoded string
            pLDS->SetValueForKey(strMoConnectOAuthTokenKey, CBaseEncoding::URLDecode(strTokenKey));
            pLDS->SetValueForKey(strMoConnectOAuthSecretKey, CBaseEncoding::URLDecode(strTokenSecret));
            pLDS->Synchronise();
		}
        //------------------------
        /// Forget Saved User Details
        //------------------------
		void CMoConnectSystem::ForgetSavedUserDetails(bool inbAnonymous)
		{
			CLocalDataStore* pLDS = CLocalDataStore::GetSingletonPtr();
			
			if(inbAnonymous)
            {
				pLDS->TryEraseKey(kstrMoConnectAnonIdKey);
				pLDS->TryEraseKey(kstrMoConnectAnonRealmKey);
                pLDS->TryEraseKey(kstrMoConnectAnonOAuthTokenKey);
                pLDS->TryEraseKey(kstrMoConnectAnonOAuthSecretKey);
			}
            else
            {
				pLDS->TryEraseKey(kstrMoConnectIdKey);
				pLDS->TryEraseKey(kstrMoConnectRealmKey);
				pLDS->TryEraseKey(kstrMoConnectUserKey);
                pLDS->TryEraseKey(kstrMoConnectOAuthTokenKey);
                pLDS->TryEraseKey(kstrMoConnectOAuthSecretKey);
			}
			
			pLDS->Synchronise();
        }
        //------------------------
        /// Sign Out Current User
        //------------------------
		void CMoConnectSystem::SignOutCurrentUser()
        {
			mbHasSignedInUser = false;
			mstrRealm = mstrMoConnectURL;
			mstrUserID.clear();
			mstrUserName.clear();
			mastrCurrentAccountLogins.clear();
            
            mstrOAuthTokenSecret = "";
            mstrOAuthToken = "";
            mpOAuthSystem->SetOAuthTokenKey("");
            mpOAuthSystem->SetOAuthTokenSecret("");
			
			OnUserChanged();
		}
        //------------------------
        /// Make Request
        //------------------------
		u32 CMoConnectSystem::MakeRequest(const std::string& instrMethod, const RequestResultDelegate& inDelegate)
        {
			Json::Value cEmptyPayload(Json::objectValue);
			return MakeRequest(instrMethod,cEmptyPayload,inDelegate);
		}
        //------------------------
        /// Make Request
        //------------------------
		u32 CMoConnectSystem::MakeRequest(const std::string& instrMethod, Json::Value& incPayload, const RequestResultDelegate& inDelegate)
        {
			RequestInfo sNewRequest;
			sNewRequest.udwID = mudwRequestIDSeed++;
			sNewRequest.Callback = inDelegate;
			sNewRequest.strMethod = instrMethod;
			sNewRequest.cPayload = incPayload;
            
			HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrRealm + instrMethod;
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			requestDetails.strBody = sNewRequest.cPayload.toUnformattedString();
            GenerateAuthenticationHeader(requestDetails.strURL, requestDetails.sHeaders);
            sNewRequest.pHttpRequest = mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this,&CMoConnectSystem::GeneralRequestCompletes));
			masOpenRequests.push_back(sNewRequest);
			
			return sNewRequest.udwID;
		}
        //------------------------
        /// Cancel Request
        //------------------------
		void CMoConnectSystem::CancelRequest(u32 inudwID)
        {
			for (u32 nRequest = 0; nRequest < masOpenRequests.size(); nRequest++)
            {
				if ( (masOpenRequests[nRequest].udwID == inudwID) && (masOpenRequests[nRequest].pHttpRequest != NULL))
                {
					masOpenRequests[nRequest].pHttpRequest->Cancel();
					break;
				}
			}
		}
        //------------------------
        /// Cancel All Requests
        //------------------------
		void CMoConnectSystem::CancelAllRequests()
        {
			mbNoRemoveFulfilledRequests = true;
			for (u32 nRequest = 0; nRequest < masOpenRequests.size(); nRequest++)
            {
                masOpenRequests[nRequest].pHttpRequest->Cancel();
			}
			masOpenRequests.clear();
			mbNoRemoveFulfilledRequests = false;
		}
        //------------------------
        /// General Request Completes
        //------------------------
		void CMoConnectSystem::GeneralRequestCompletes(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
			RequestResult eRequestResult = RequestResult::k_success;
			Json::Value cJResponse;
			RequestInfo* pRequest = FindRequestWithHttpRequest(inpRequest);
			
			// This shouldn't ever happen but if it does let's sidestep over the issue.
			if(NULL == pRequest)
            {
				return;
			}
			
			u32 udwRequestId = pRequest->udwID;
			
			switch(ineResult)
            {
				case IHttpRequest::CompletionResult::k_cancelled:
					eRequestResult = RequestResult::k_cancelled;
					break;
				case IHttpRequest::CompletionResult::k_timeout:
					eRequestResult = RequestResult::k_failedNoResponse;
					break;
				case IHttpRequest::CompletionResult::k_failed:
					eRequestResult = RequestResult::k_failedNoResponse;
					break;
                default:
                    break;
			}
			
			if(inpRequest->GetResponseCode() == kHTTPRedirect)
            {
				HandleRedirection(inpRequest);
				HttpRequestDetails sDetails = inpRequest->GetDetails();
				sDetails.strURL = mstrRealm + "/logins";
				mpPendingLoginsRequest = mpHttpConnectionSystem->MakeRequest(sDetails, inpRequest->GetCompletionDelegate());
				return;
			}
            else
            {
				Json::Reader cReader;
				if(cReader.parse(inpRequest->GetResponseString(),cJResponse))
                {
					if(cJResponse.isObject() && cJResponse.isMember("Error"))
                    {
						if(inpRequest->GetResponseCode() == 503)
                        {
							eRequestResult = RequestResult::k_failedInternalServerError;
						}
                        else
                        {
							eRequestResult = RequestResult::k_failedClientError;
						}
					}
				}
			}
			
			pRequest->pHttpRequest = NULL;
			RequestResultDelegate reqCallback = pRequest->Callback;
			if(reqCallback)
            {
				
				reqCallback(udwRequestId,eRequestResult,cJResponse);
			}
			
			// Reassigning this as we could have issued a new request in
            // response to the above callback causing shenanigans with reallocation
			pRequest = FindRequestWithID(udwRequestId);
			
			if(!mbNoRemoveFulfilledRequests && pRequest)
            {
				std::remove(masOpenRequests.begin(),masOpenRequests.end(),*pRequest);
			}
		}
		//------------------------
        /// Find Request With ID
        //------------------------
		CMoConnectSystem::RequestInfo* CMoConnectSystem::FindRequestWithID(u32 inudwID)
        {
			for(u32 udwReq = 0; udwReq < masOpenRequests.size(); udwReq++)
            {
                if(masOpenRequests[udwReq].udwID == inudwID)
                {
					return &masOpenRequests[udwReq];
				}
			}
			
			return NULL;
		}
        //------------------------
        /// Find Request With Http Request
        //------------------------
		CMoConnectSystem::RequestInfo* CMoConnectSystem::FindRequestWithHttpRequest(HttpRequestPtr inpHttp)
        {
			for(u32 udwReq = 0; udwReq < masOpenRequests.size(); udwReq++)
            {
				if(masOpenRequests[udwReq].pHttpRequest == inpHttp)
                {
					return &masOpenRequests[udwReq];
				}
			}
			
			return NULL;
		}
        //------------------------
        /// Validate IAP Receipt
        //------------------------
        void CMoConnectSystem::ValidateIAPReceipt(const IAPType ineType,
                                                  const IAPTransactionPtr& inpTransInfo,
                                                  ValidateReceiptDelegate inDelegate)
        {
            DEBUG_LOG("CMoConnectSystem::ValidateIAPReceipt");
            HttpRequestDetails requestDetails;

            requestDetails.strURL = mstrRealm + "/iap/production";
            
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			
			Json::Value cIAPDetails(Json::objectValue);
			cIAPDetails["Service"]     = GetIAPTypeAsString(ineType) + kstrEnvironment;
			cIAPDetails["ReceiptID"]   = inpTransInfo->strTransactionID;
			cIAPDetails["Receipt"]     = inpTransInfo->strReceipt;
            
            Json::Value cIAPMsg(Json::objectValue);
			cIAPMsg["Data"] = cIAPDetails;
      
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            DEBUG_LOG(strOAuthHeader);
            
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			Json::FastWriter cWriter;
			requestDetails.strBody = cWriter.write(cIAPMsg);
			DEBUG_LOG("ValidateIAPReceipt:"+requestDetails.strBody);
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::OnIAPRecieptValidationResponse));
            
            mValidateReceiptDelegate = inDelegate;
        }
        //------------------------
        /// Get IAP Type As String
        //------------------------
        const std::string& CMoConnectSystem::GetIAPTypeAsString(const IAPType ineType)
        {
            switch(ineType)
            {
                case IAPType::k_apple:
                    return kstrIAPApple;
                case IAPType::k_google:
                    return kstrIAPnGoogle;
                default:
                    ERROR_LOG("Unsupported push notification type!");
                    break;
            }
            
            return kstrIAPUnknown;
        }
        //------------------------
        /// On IAP Reciept Validation Response
        //------------------------
        void CMoConnectSystem::OnIAPRecieptValidationResponse(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
            bool bIsValid = false;
            IAPReceipt sIAP;
            sIAP.ddwTimeCreated = 0;
            sIAP.bRedeemed = false;
            
            if(IHttpRequest::CompletionResult::k_completed == ineResult)
            {
                DEBUG_LOG("CMoConnectSystem::OnRecieptValidationResponse");
                if(kHTTPResponseOK == inpRequest->GetResponseCode())
                {
                    Json::Reader cReader;
                    Json::Value cJResponse;
                    if(cReader.parse(inpRequest->GetResponseString(), cJResponse))
                    {
                        if(cJResponse.isObject() && cJResponse.isMember("Error"))
                        {
                            // Error
                            DEBUG_LOG("CMoConnectSystem::OnRecieptValidationResponse() - This ain't right!");
                        }
                        else
                        {
                            // Construct our
                            if(cJResponse.isMember("IAPRecordID"))
                            {
                                sIAP.strRecordId = cJResponse["IAPRecordID"].asString();
                            }
                            if(cJResponse.isMember("UserID"))
                            {
                                sIAP.strUserId = cJResponse["UserID"].asString();
                            }
                            if(cJResponse.isMember("Service"))
                            {
                                sIAP.strService = cJResponse["Service"].asString();
                            }
                            if(cJResponse.isMember("ReceiptID"))
                            {
                                sIAP.strReceiptId = cJResponse["ReceiptID"].asString();
                            }
                            if(cJResponse.isMember("DateCreated"))
                            {
                                sIAP.ddwTimeCreated = cJResponse["DateCreated"].asInt();
                            }
                            if(cJResponse.isMember("StoreID"))
                            {
                                sIAP.strProductId = cJResponse["StoreID"].asString();
                            }
                            if(cJResponse.isMember("Status"))
                            {
                                sIAP.strStatus = cJResponse["Status"].asString();
                            }
                            if(cJResponse.isMember("Redeemed"))
                            {
                                sIAP.bRedeemed = cJResponse["Redeemed"].asInt() == 1;
                            }
                            bIsValid = true;
                        }
                    }
                }
                else
                {
                    ERROR_LOG("Unable to validate IAP receipt.\nGot response code \""+ToString(inpRequest->GetResponseCode())+"\"");
                }
            }
            else
            {
                ERROR_LOG("Unable to validate IAP receipt as HTTP request did not complete. Instead we got result: "+ToString((u32)ineResult));
            }
            
            mValidateReceiptDelegate(bIsValid, ineResult, sIAP);
        }
        //------------------------
        /// Redeem IAP
        //------------------------
        void CMoConnectSystem::RedeemIAP(const std::string& instrReceiptId)
        {
            DEBUG_LOG("CMoConnectSystem::RedeemIAP");
            HttpRequestDetails requestDetails;
			requestDetails.strURL = mstrRealm + "/iap/redeem";
			requestDetails.eType = ChilliSource::Networking::HttpRequestDetails::Type::k_post;
			
			Json::Value cIAPDetails(Json::objectValue);
			cIAPDetails["IAPRecordID"] = instrReceiptId;
            
            Json::Value cIAPMsg(Json::objectValue);
			cIAPMsg["Data"] = cIAPDetails;
            
            std::string strOAuthHeader;
            mpOAuthSystem->GetOAuthHeader(Core::COAuthSystem::OAuthHttpRequestType::k_httpPost, requestDetails.strURL, "", strOAuthHeader);
            DEBUG_LOG(strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Authorization", strOAuthHeader);
            requestDetails.sHeaders.SetValueForKey("Content-Type", "application/json");
			
			Json::FastWriter cWriter;
			requestDetails.strBody = cWriter.write(cIAPMsg);
			DEBUG_LOG("RedeemIAP:"+requestDetails.strBody);
			mpHttpConnectionSystem->MakeRequest(requestDetails, IHttpRequest::CompletionDelegate(this, &CMoConnectSystem::OnIAPRedeemedResponse));
        }
        //------------------------
        /// On IAP Redeemed Response
        //------------------------
        void CMoConnectSystem::OnIAPRedeemedResponse(HttpRequestPtr inpRequest, IHttpRequest::CompletionResult ineResult)
        {
            if(IHttpRequest::CompletionResult::k_completed == ineResult)
            {
                Json::Reader cReader;
                Json::Value cJResponse;
                if(cReader.parse(inpRequest->GetResponseString(), cJResponse))
                {
                    if(!cJResponse.isNull())
                    {
                        ERROR_LOG("Unable to redeem iap.");
                    }
                }
            }
        }
	}
}
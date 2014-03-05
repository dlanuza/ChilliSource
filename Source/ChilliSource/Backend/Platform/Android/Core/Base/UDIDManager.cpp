/*
 *  UDIDManager.cpp
 *  MoFlow
 *
 *  Created by Ian Copland on 01/12/2011.
 *  Copyright 2011 Tag Games. All rights reserved.
 *
 */

#include <ChilliSource/Backend/Platform/Android/Core/Base/UDIDManager.h>

#include <ChilliSource/Backend/Platform/Android/Core/Base/CoreJavaInterface.h>
#include <ChilliSource/Backend/Platform/Android/Core/File/SharedPreferencesJavaInterface.h>
#include <ChilliSource/Backend/Platform/Android/Core/JNI/JavaInterfaceManager.h>
#include <ChilliSource/Core/Cryptographic/HashMD5.h>
#include <ChilliSource/Core/File/LocalDataStore.h>

#include <cstdlib>
#include <ctime>

namespace ChilliSource
{
	namespace Android
	{
		const std::string kstrUDIDStorageKey = "UDID";
		const std::string kstrSharedPrefsDocName = "MoFlowPreferences";
		//-----------------------------------------
		/// Constructor
		//-----------------------------------------
		UDIDManager::UDIDManager() : mstrUDID("NoUDID"), mbInitialised(false)
		{
		}
		//-----------------------------------------
		/// Get UDID
		//-----------------------------------------
		std::string UDIDManager::GetUDID()
		{
			if (mbInitialised == false)
				Initialise();

			return mstrUDID;
		}
		//-----------------------------------------
		/// Initialise
		//-----------------------------------------
		void UDIDManager::Initialise()
		{
			if (LoadUDID() == false)
				CalculateUDID();

			CS_LOG_DEBUG("UDID: " + mstrUDID);
			mbInitialised = true;
		}
		//-----------------------------------------
		/// Load UDID
		//-----------------------------------------
		bool UDIDManager::LoadUDID()
		{
			//load the UDID from the local data store
			std::string strLDSUDID = "";
			bool bLDSUDIDExists = false;
			if (Core::LocalDataStore::GetSingletonPtr()->HasValueForKey(kstrUDIDStorageKey) == true)
			{
				bLDSUDIDExists = Core::LocalDataStore::GetSingletonPtr()->TryGetValue(kstrUDIDStorageKey, strLDSUDID);
			}

			//load the UDID from the android data store
			std::string strASPUDID = "";
			bool bASPUDIDExists = false;
			if (SharedPreferencesJavaInterface::KeyExists(kstrSharedPrefsDocName, kstrUDIDStorageKey) == true)
			{
				strASPUDID = SharedPreferencesJavaInterface::GetString(kstrSharedPrefsDocName, kstrUDIDStorageKey, "FailedToGetUDID");
				if (strASPUDID != "FailedToGetUDID")
					bASPUDIDExists = true;
			}

			//if neither succeed, or they are different, then return false, otherwise set the UDID.
			if ((bLDSUDIDExists == false && bASPUDIDExists == false) || (bLDSUDIDExists == true && bASPUDIDExists == true && strLDSUDID != strASPUDID))
				return false;

			if (bLDSUDIDExists == true)
				mstrUDID = strLDSUDID;
			else if (bASPUDIDExists == true)
				mstrUDID = strASPUDID;
			else
				CS_LOG_FATAL("Something has gone seriously wrong with the loading of UDIDs :S");

			return true;
		}
		//-----------------------------------------
		/// Save UDID
		//-----------------------------------------
		void UDIDManager::SaveUDID()
		{
			//store in the local data store
			Core::LocalDataStore::GetSingletonPtr()->SetValueForKey(kstrUDIDStorageKey, mstrUDID);
			Core::LocalDataStore::GetSingletonPtr()->Synchronise();

			//store in shared preferences
			SharedPreferencesJavaInterface::SetString(kstrSharedPrefsDocName, kstrUDIDStorageKey, mstrUDID);
		}
		//-----------------------------------------
		/// Calculate UDID
		//-----------------------------------------
		void UDIDManager::CalculateUDID()
		{
			CoreJavaInterfacePtr pCoreJI = JavaInterfaceManager::GetSingletonPtr()->GetJavaInterface<CoreJavaInterface>();

			//--try the mac address
			std::string strMacAddress = pCoreJI->GetMacAddress();
			if (strMacAddress != "")
			{
				mstrUDID = "m-" + Core::HashMD5::GenerateHexHashCode(strMacAddress);
				SaveUDID();
				return;
			}

			//--try the Android ID
			std::string strAndroidID = pCoreJI->GetAndroidID();
			if (strAndroidID != "")
			{
				mstrUDID = "a-" + Core::HashMD5::GenerateHexHashCode(strAndroidID);
				SaveUDID();
				return;
			}

			//--try the ID from the telephony manager
			std::string strTelephonyID = pCoreJI->GetTelephonyDeviceID();
			if (strTelephonyID != "")
			{
				mstrUDID = "t-" + Core::HashMD5::GenerateHexHashCode(strTelephonyID);
				SaveUDID();
				return;
			}

			//--if all this fails, fall back on generating a random hash.
			//first of all seed rand with something that is as unique as possible: allocate a random bit of memory and add this to current time.
			u32* pRandomMemory = new u32;
			u32 udwSeed = ((u32)pRandomMemory) + time(NULL);
			CS_SAFEDELETE(pRandomMemory);
			srand(udwSeed);

			//generate a random number. perform a sequence of rand() calls incase MAX_RAND is really low.
			u32 dwRandomNumber = 0;
			u32 dwMaxUnsignedInt = ((u32)2147483647);
			u32 dwNumberOfRands = (u32)(RAND_MAX / dwMaxUnsignedInt);
			for (u32 i = 0; i < dwNumberOfRands; ++i)
				dwRandomNumber += rand();

			//use this random number to generate a UDID
			mstrUDID = "r-" + Core::HashMD5::GenerateHexHashCode(Core::ToString(dwRandomNumber));
			SaveUDID();
		}
		//-----------------------------------------
		/// Deconstructor
		//-----------------------------------------
		CUDIDManager::~CUDIDManager()
		{
		}
	}
}
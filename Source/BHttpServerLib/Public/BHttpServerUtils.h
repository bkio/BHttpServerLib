/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#if PLATFORM_LINUX
#include "../Private/Linux/include/CivetServer.h"
#elif PLATFORM_MAC
#include "../Private/Mac/include/CivetServer.h"
#elif PLATFORM_WINDOWS
#include "CivetServer.h"
#endif

namespace BHttpServerUtils
{
	/** Read post data from the connection */
	FString GetPostData(mg_connection* RequestConnection);

	/** Read URL data from the connection */
	FString GetURLFromGetRequest(mg_connection* RequestConnection);

} // namespace BHttpServerUtils
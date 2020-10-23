/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"

class BHTTPSERVERLIB_API BHttpServer
{

public:
	BHttpServer();
	~BHttpServer();

	/** Start web server */
	bool StartServer(const FString& ServerURL);

	/** Stop web server and unbind all handlers */
	bool StopServer();

	/** Whether the server is running */
	bool IsRunning() const;

	/** Add new handler to process URI */
	bool AddHandler(TSharedPtr<class BKHttpServerHandler> Handler, const FString& URI);

	/** Remove handler from URI */
	bool RemoveHandler(const FString& URI);

private:
	/** All added handlers <URI, Handler> */
	TMap<FString, TSharedPtr<class BKHttpServerHandler>> BinnedHandlers;

	/** Pointer to the native CivetWeb implementation instance */
	TSharedPtr<class CivetServer> Server;
};
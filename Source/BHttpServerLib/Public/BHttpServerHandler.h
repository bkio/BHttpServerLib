/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Event.h"
#include "Misc/Guid.h"
#if PLATFORM_LINUX
#include "../Private/Linux/include/CivetServer.h"
#elif PLATFORM_MAC
#include "../Private/Mac/include/CivetServer.h"
#elif PLATFORM_WINDOWS
#include "CivetServer.h"
#endif

/** Request context */
struct FContext
{
	mg_connection* Connection;
	bool bFinished = false;
	FString Response;
	FEvent* ReadyEvent;

	FContext(mg_connection* InConnection, FEvent* InReadyEvent)
		: Connection(InConnection)
		, ReadyEvent(InReadyEvent)
	{
		check(InConnection);
		check(InReadyEvent);
	}
};

/** Handler of POST requests on the specific URI */
class BHTTPSERVERLIB_API BKHttpServerHandler : public CivetHandler
{

public:
	BKHttpServerHandler();

	/** Override it with custom processing logic */
	virtual void ProcessRequest(const FGuid& RequestId, const FString& RequestData);

	/** Finish request processing with given response data */
	void ProcessRequestFinish(const FGuid& RequestId, const FString& ResponseData);

	/** Whether the request is in processing */
	bool IsProcessing() const;

	/** Whether the current request is aborted */
	bool IsAborted() const;

	/** Sets optional header info (should be global for handler) */
	void SetHeader(const FString& HeaderName, const FString& HeaderValue);

	/** Get header value for given request id */
	FString GetHeader(const FGuid& RequestId, const FString& HeaderName) const;

	/** Get handler URI value */
	FString GetURI() const;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnResponseCompleted, const FGuid&)
	FOnResponseCompleted OnResponseCompleted;

	/** Handle POST requests */
	virtual bool handlePost(CivetServer* Server, mg_connection* RequestConnection) override;

	/** Handle GET requests */
	virtual bool handleGet(CivetServer* Server, struct mg_connection* RequestConnection) override;
	
	/** Not used **/
	virtual bool handleHead(CivetServer* server, struct mg_connection* conn) override { return false; }
	virtual bool handlePut(CivetServer* server, struct mg_connection* conn) override { return false; }
	virtual bool handleDelete(CivetServer* server, struct mg_connection* conn) override { return false; }
	virtual bool handleOptions(CivetServer* server, struct mg_connection* conn) override { return false; }
	virtual bool handlePatch(CivetServer* server, struct mg_connection* conn) override { return false; }

private:
	friend class BHttpServer;

	/** Called from wrapped to bind handler to server */
	bool BindHandler(const FString& URI, TSharedPtr<class CivetServer> ServerImpl);

	/** Cached handler URI */
	FString HandlerURI;

	/** Internal binned status */
	bool bHandlerBinned;

	/** Processing in progress flag */
	bool bProcessing;

	/** Aborted flag */
	bool bAborted;

	/** Request processing on the game thread */
	void ProcessRequest_OnGameThread_Preensured(const FGuid& RequestId, const FString& RequestData);

	/** Start request processing */
	void OnRequest(const FGuid& RequestId, const FString& RequestData);

	/** Abort async request processing */
	void AbortAsyncRequest(const FGuid& RequestId);

	/** Create a new request context */
	FEvent* CreateContext(mg_connection* RequestConnection, const FGuid& RequestId);

	/** Wait the response from the request processing or timeout */
	bool WaitForResponse(mg_connection* RequestConnection, FEvent* RequestReadyEvent, const FGuid& RequestId);

	/** Get response data for the given request id */
	FString GetResponseData(const FGuid& RequestId, bool bTimeout);

	/** Send given response with status code to the request connection */
	bool SendResponse(mg_connection* RequestConnection, const FGuid& RequestId, const FString& StatusCode, const FString& ResponseData);

	/** Write given data string into the request connection */
	bool Write(mg_connection* RequestConnection, const FString& Data);

	/** Write given data string into the request connection using chunks */
	bool WriteChunked(mg_connection* RequestConnection, const FString& Data);

	/** Critical section used to lock the request data for access */
	mutable FCriticalSection CriticalSection;

	/** Processed request contexts map */
	TMap<FGuid, FContext> Contexts;

	/** Request headers that will be added to the response */
	TMap<FString, FString> ResponseHeaders;

	/** Request headers cached as a string */
	FString CachedResponseHeaders;

	/** Print given headers into the single string */
	FString PrintHeadersToString(const TMap<FString, FString>& Headers);
};
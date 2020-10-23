/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BHttpServerHandler.h"
#include "BHttpServer.h"
#include "BHttpServerDefines.h"
#include "BHttpServerUtils.h"
#include "Async/TaskGraphInterfaces.h"

/*
Game-thread part
*/

BKHttpServerHandler::BKHttpServerHandler()
	: bProcessing(false)
	, bAborted(false)
{
	ResponseHeaders.Emplace(TEXT("Server"), TEXT("Http Server"));
	ResponseHeaders.Emplace(TEXT("Content-Type"), TEXT("application/json"));
	ResponseHeaders.Emplace(TEXT("Connection"), TEXT("close"));
	ResponseHeaders.Emplace(TEXT("Transfer-Encoding"), TEXT("chunked"));
	CachedResponseHeaders = PrintHeadersToString(ResponseHeaders);
}

void BKHttpServerHandler::ProcessRequest(const FGuid& RequestId, const FString& RequestData)
{
	ProcessRequestFinish(RequestId, RequestData);
}

void BKHttpServerHandler::ProcessRequestFinish(const FGuid& RequestId, const FString& ResponseData)
{
	if (bAborted)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: request was aborted"), *HTTP_SERVER_FUNC_LINE);
		return;
	}

	check(bProcessing);
	bProcessing = false;

	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id '%s'"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	FScopeLock Lock(&CriticalSection);

	const auto ContextPtr = Contexts.Find(RequestId);
	if (!ContextPtr)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: No GUID %s is found for processing request (RequestTimeout)"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());
		return;
	}

	auto& Context = *ContextPtr;
	if (Context.bFinished)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: request processing is already finished"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());
		return;
	}

	Context.bFinished = true;
	Context.Response = ResponseData;

	const auto ReadyEvent = Context.ReadyEvent;
	ReadyEvent->Trigger();
}

bool BKHttpServerHandler::IsProcessing() const
{
	return bProcessing;
}

bool BKHttpServerHandler::IsAborted() const
{
	return bAborted;
}

void BKHttpServerHandler::SetHeader(const FString& HeaderName, const FString& HeaderValue)
{
	if (bHandlerBinned)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: Can't set handler header: it's already binned"), *HTTP_SERVER_FUNC_LINE);
		return;
	}

	FScopeLock Lock(&CriticalSection);

	ResponseHeaders.Emplace(HeaderName, HeaderValue);
	CachedResponseHeaders = PrintHeadersToString(ResponseHeaders);
}

FString BKHttpServerHandler::GetHeader(const FGuid& RequestId, const FString& HeaderName) const
{
	check(bProcessing);

	FScopeLock Lock(&CriticalSection);

	const auto ContextPtr = Contexts.Find(RequestId);
	if (!ContextPtr)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: No GUID %s is found for processing request (RequestTimeout)"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());
		return FString{};
	}

	const auto& Context = *ContextPtr;

	const auto RequestConnection = Context.Connection;
	std::string HeaderNameStd = std::string(TCHAR_TO_UTF8(*HeaderName));

	const auto ValueCStr = CivetServer::getHeader(RequestConnection, HeaderNameStd);
	return FString{ ValueCStr };
}

FString BKHttpServerHandler::GetURI() const
{
	return HandlerURI;
}

bool BKHttpServerHandler::BindHandler(const FString& URI, TSharedPtr<CivetServer> ServerImpl)
{
	check(ServerImpl.IsValid());
	check(!URI.IsEmpty());

	// Cache ServerWrapper pointer and its URI (it's required by BKHttpServerHandler::BeginDestroy())
	HandlerURI = URI;

	// Bind handler to civet server internal instance
	ServerImpl->addHandler(TCHAR_TO_ANSI(*HandlerURI), this);

	// Set as binned
	bHandlerBinned = true;

	return true;
}

void BKHttpServerHandler::ProcessRequest_OnGameThread_Preensured(const FGuid& RequestId, const FString& RequestData)
{
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id %s"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	OnRequest(RequestId, RequestData);
}

void BKHttpServerHandler::OnRequest(const FGuid& RequestId, const FString& RequestData)
{
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id '%s'"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	check(!bProcessing);
	bProcessing = true;
	bAborted = false;

	ProcessRequest(RequestId, RequestData);

	if (IsProcessing())
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: async handlers is not allowed"), *HTTP_SERVER_FUNC_LINE);
		AbortAsyncRequest(RequestId);
	}
}

void BKHttpServerHandler::AbortAsyncRequest(const FGuid& RequestId)
{
	UE_LOG(LogBHttpServer, Warning, TEXT("%s: request id %s"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	check(!bAborted);
	check(bProcessing);

	const auto Response = TEXT("Aborted");
	ProcessRequestFinish(RequestId, Response);

	bAborted = true;
}

/*
Threaded part
*/

bool BKHttpServerHandler::handlePost(CivetServer* Server, mg_connection* RequestConnection)
{
	// Unique request id
	const FGuid RequestId = FGuid::NewGuid();

	FEvent* const RequestReadyEvent = CreateContext(RequestConnection, RequestId);

	FString PostData = BHttpServerUtils::GetPostData(RequestConnection);

	// Set request processing on the game thread
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, PostData, RequestId]()
		{
			ProcessRequest_OnGameThread_Preensured(RequestId, PostData);
		}, TStatId(), nullptr, ENamedThreads::GameThread);

	bool bResult = WaitForResponse(RequestConnection, RequestReadyEvent, RequestId);

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, RequestId]()
		{
			OnResponseCompleted.Broadcast(RequestId);
		}, TStatId(), nullptr, ENamedThreads::GameThread);

	return bResult;
}

bool BKHttpServerHandler::handleGet(CivetServer* server, mg_connection* RequestConnection)
{
	// Unique request id
	const FGuid RequestId = FGuid::NewGuid();

	FEvent* const RequestReadyEvent = CreateContext(RequestConnection, RequestId);

	FString GetURI = BHttpServerUtils::GetURLFromGetRequest(RequestConnection);

	// Set request processing on the game thread
	FFunctionGraphTask::CreateAndDispatchWhenReady([this, RequestId, GetURI]()
		{
			ProcessRequest_OnGameThread_Preensured(RequestId, GetURI);
		}, TStatId(), nullptr, ENamedThreads::GameThread);

	bool bResult = WaitForResponse(RequestConnection, RequestReadyEvent, RequestId);

	FFunctionGraphTask::CreateAndDispatchWhenReady([this, RequestId]()
		{
			OnResponseCompleted.Broadcast(RequestId);
		}, TStatId(), nullptr, ENamedThreads::GameThread);

	return bResult;
}

FEvent* BKHttpServerHandler::CreateContext(mg_connection* RequestConnection, const FGuid& RequestId)
{
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id %s"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	// Create async event waiter
	FEvent* const RequestReadyEvent = FGenericPlatformProcess::GetSynchEventFromPool();

	// Add a new request context

	FScopeLock Lock(&CriticalSection);
	Contexts.Add(RequestId, FContext(RequestConnection, RequestReadyEvent));

	return RequestReadyEvent;
}

bool BKHttpServerHandler::WaitForResponse(mg_connection* RequestConnection, FEvent* RequestReadyEvent, const FGuid& RequestId)
{
	// Wait for event or timeout
	const int32 WaitTime = 20000;
	const bool bIgnoreThreadIdleStats = true;
	const bool bEventTriggered = RequestReadyEvent->Wait(WaitTime, bIgnoreThreadIdleStats);
	FGenericPlatformProcess::ReturnSynchEventToPool(RequestReadyEvent);

	const bool bTimeout = !bEventTriggered;
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id %s: wait is over: timeout %d"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString(), static_cast<int32>(bTimeout));

	FString ResponseData = GetResponseData(RequestId, bTimeout);

	const FString StatusCode = !bTimeout ? TEXT("200 OK") : TEXT("503 Service Unavailable");

	return SendResponse(RequestConnection, RequestId, StatusCode, ResponseData);
}

FString BKHttpServerHandler::GetResponseData(const FGuid& RequestId, bool bTimeout)
{
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id %s"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	FScopeLock Lock(&CriticalSection);
	auto Context = Contexts.FindAndRemoveChecked(RequestId);

	if (bTimeout)
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: request id %s: timeout reached"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

		return TEXT("Timeout");
	}

	return Context.Response;
}

bool BKHttpServerHandler::SendResponse(mg_connection* RequestConnection, const FGuid& RequestId, const FString& StatusCode, const FString& ResponseData)
{
	UE_LOG(LogBHttpServer, Verbose, TEXT("%s: request id %s"), *HTTP_SERVER_FUNC_LINE, *RequestId.ToString());

	// Update unique per-request params
	TMap<FString, FString> AdditionalHeaders;
	AdditionalHeaders.Emplace(TEXT("X-Request-ID"), RequestId.ToString());

	FString ResponseHeader = FString::Printf(TEXT("HTTP/1.1 %s\r\n"), *StatusCode);
	ResponseHeader.Append(CachedResponseHeaders);
	ResponseHeader.Append(PrintHeadersToString(AdditionalHeaders));
	ResponseHeader.Append("\r\n");

	if (!Write(RequestConnection, ResponseHeader))
	{
		return false;
	}

	if (!WriteChunked(RequestConnection, ResponseData))
	{
		return false;
	}

	return true;
}

bool BKHttpServerHandler::Write(mg_connection* RequestConnection, const FString& Data)
{
	// Convert to UTF-8
	const auto Converter = FTCHARToUTF8(*Data);
	const char* const Ptr = Converter.Get();
	const size_t Len = Converter.Length();

	return mg_write(RequestConnection, Ptr, Len) > 0;
}

bool BKHttpServerHandler::WriteChunked(mg_connection* RequestConnection, const FString& Data)
{
	const int32 ChunkSize = 10240;

	for (int32 i = 0; i < Data.Len(); i += ChunkSize)
	{
		const int32 Size = FMath::Min(Data.Len(), i + ChunkSize) - i;
		const auto& Array = Data.GetCharArray();

		const auto* const DataPtr = Array.GetData() + i;

		// Convert to UTF-8
		const auto Converter = FTCHARToUTF8(DataPtr, Size);
		const char* const Ptr = Converter.Get();
		const size_t Len = Converter.Length();

		// Send a chunk of data
		if (mg_send_chunk(RequestConnection, Ptr, Len) <= 0)
		{
			UE_LOG(LogBHttpServer, Error, TEXT("%s: cannot write data in the connection"), *HTTP_SERVER_FUNC_LINE);
			return false;
		}
	}

	// Send a trailer
	const char Trailer[] = "0\r\n\r\n";
	const size_t TrailerSize = sizeof(Trailer) - 1;
	return mg_write(RequestConnection, Trailer, TrailerSize) > 0;
}

FString BKHttpServerHandler::PrintHeadersToString(const TMap<FString, FString>& Headers)
{
	FString OutputStr;
	for (auto& Header : Headers)
	{
		OutputStr.Append(Header.Key + ": " + Header.Value + "\r\n");
	}

	return OutputStr;
}
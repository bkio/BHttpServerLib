/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#include "BHttpServer.h"
#include "BHttpServerDefines.h"
#include "BHttpServerHandler.h"
#if PLATFORM_LINUX
#include "../Private/Linux/include/CivetServer.h"
#elif PLATFORM_MAC
#include "../Private/Mac/include/CivetServer.h"
#elif PLATFORM_WINDOWS
#include "CivetServer.h"
#endif

BHttpServer::BHttpServer()
{
}

BHttpServer::~BHttpServer()
{
	StopServer();
}

bool BHttpServer::StartServer(const FString& ServerURL)
{
	std::vector<std::string> Options;
	Options.push_back("listening_ports");
	Options.push_back(TCHAR_TO_ANSI(*ServerURL));
	Options.push_back("num_threads");
	Options.push_back("1");

	Options.push_back("enable_keep_alive");
	Options.push_back("no");
	Options.push_back("keep_alive_timeout_ms");
	Options.push_back("0");

	// Stop server first if one already exists
	if (IsRunning())
	{
		UE_LOG(LogBHttpServer, Warning, TEXT("%s: Existing CivetWeb server instance found, stop it now"), *HTTP_SERVER_FUNC_LINE);
		StopServer();
	}

	// Run new civet instance
	try
	{
		Server = TSharedPtr<CivetServer>(new CivetServer(Options));
	}
	catch (CivetException* e)
	{
		UE_LOG(LogBHttpServer, Fatal, TEXT("%s: CivetWeb exception (StartServer): %s"), e->what());
		return false;
	}

	// Check that we're really running now
	if (Server->getContext() == nullptr)
	{
		UE_LOG(LogBHttpServer, Fatal, TEXT("%s: Failed to run CivetWeb server instance"), *HTTP_SERVER_FUNC_LINE);
		return false;
	}

	UE_LOG(LogBHttpServer, Log, TEXT("%s: CivetWeb server instance started on: %s"), *HTTP_SERVER_FUNC_LINE, *ServerURL);
	return true;
}

bool BHttpServer::StopServer()
{
	if (Server.IsValid())
	{
		Server->close();

		Server.Reset();

		UE_LOG(LogBHttpServer, Log, TEXT("%s: CivetWeb server instance stopped"), *HTTP_SERVER_FUNC_LINE);
	}
	return true;
}

bool BHttpServer::IsRunning() const
{
	return Server.IsValid();
}

bool BHttpServer::AddHandler(TSharedPtr<BKHttpServerHandler> Handler, const FString& URI)
{
	if (URI.IsEmpty())
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: Can't add hadler: URI is empty"), *HTTP_SERVER_FUNC_LINE);
		return false;
	}

	if (!IsRunning())
	{
		UE_LOG(LogBHttpServer, Error, TEXT("%s: Can't add hadler: you should run server first"), *HTTP_SERVER_FUNC_LINE);
		return false;
	}

	// Remove previous handler if we had one
	RemoveHandler(URI);

	if (Handler->BindHandler(URI, Server))
	{
		BinnedHandlers.Emplace(URI, Handler);

		UE_LOG(LogBHttpServer, Log, TEXT("%s: Handler successfully added for URI: %s"), *HTTP_SERVER_FUNC_LINE, *URI);
		return true;
	}
	return false;
}

bool BHttpServer::RemoveHandler(const FString& URI)
{
	if (Server.IsValid())
	{
		if (BinnedHandlers.Contains(URI))
		{
			BinnedHandlers.Remove(URI);
		}

		// Remove handler from civet anyway
		Server->removeHandler(TCHAR_TO_ANSI(*URI));

		return true;
	}
	return false;
}
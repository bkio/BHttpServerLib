/// MIT License, Copyright Burak Kara, burak@burak.io, https://en.wikipedia.org/wiki/MIT_License

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBHttpServer, Log, All);

#define HTTP_SERVER_FUNC (FString(__FUNCTION__))              // Current Class Name + Function Name where this is called
#define HTTP_SERVER_LINE (FString::FromInt(__LINE__))         // Current Line Number in the code where this is called
#define HTTP_SERVER_FUNC_LINE (HTTP_SERVER_FUNC + "(" + HTTP_SERVER_LINE + ")") // Current Class and Line Number where this is called!
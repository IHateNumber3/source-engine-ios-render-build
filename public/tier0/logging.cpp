//========= Minimal original LoggingSystem_* implementation =========
//
// logging.h (from the Alien Swarm SDK) only declares these functions --
// the real implementation wasn't available in that SDK drop. This is a
// small, from-scratch stand-in: channels are tracked in a simple array,
// and Log()/LogDirect() just printf to the console (with ANSI color codes
// where supported, otherwise plain text). No file logging, no listener
// chaining beyond a single registered listener, no thread-local state --
// just enough to satisfy the linker and give readable colored console
// output for Log_Msg/Log_Warning/DevMsg-style calls used elsewhere.
//
//=====================================================================

#include "tier0/logging.h"
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace
{
	struct LoggingChannel_t
	{
		char					m_szName[64];
		LoggingSeverity_t		m_MinSeverity;
		Color					m_Color;
		LoggingChannelFlags_t	m_Flags;
	};

	const int MAX_LOGGING_CHANNELS = 256;
	LoggingChannel_t g_Channels[MAX_LOGGING_CHANNELS];
	int g_nChannelCount = 0;

	ILoggingListener *g_pListener = nullptr;

	// Basic ANSI color codes -- degrade gracefully (no-op) on terminals
	// that don't support them; we don't try to detect that here, just
	// emit them, since most CI/console output either honors them or
	// ignores the escape sequence harmlessly.
	const char *ColorToAnsi( Color c )
	{
		// Very rough bucketing into the 8 standard ANSI colors based on
		// which channel is dominant -- good enough for "is this an error
		// or a warning" at a glance, not trying to be exact.
		int r = c.r(), g = c.g(), b = c.b();
		if ( r > 180 && g < 100 && b < 100 )  return "\033[31m"; // red
		if ( g > 180 && r < 100 && b < 100 )  return "\033[32m"; // green
		if ( r > 180 && g > 180 && b < 100 )  return "\033[33m"; // yellow
		if ( b > 180 && r < 100 && g < 100 )  return "\033[34m"; // blue
		return "\033[0m"; // default/no color
	}
}

PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_RegisterLoggingChannel( const char *pName, RegisterTagsFunc registerTagsFunc, int flags, LoggingSeverity_t severity, Color color )
{
	// Already registered? Return the existing ID instead of duplicating.
	for ( int i = 0; i < g_nChannelCount; ++i )
	{
		if ( strcmp( g_Channels[i].m_szName, pName ) == 0 )
			return i;
	}

	if ( g_nChannelCount >= MAX_LOGGING_CHANNELS )
		return INVALID_LOGGING_CHANNEL_ID;

	LoggingChannelID_t id = g_nChannelCount++;
	strncpy( g_Channels[id].m_szName, pName, sizeof( g_Channels[id].m_szName ) - 1 );
	g_Channels[id].m_szName[ sizeof( g_Channels[id].m_szName ) - 1 ] = '\0';
	g_Channels[id].m_MinSeverity = severity;
	g_Channels[id].m_Color = color;
	g_Channels[id].m_Flags = (LoggingChannelFlags_t)flags;

	return id;
}

PLATFORM_INTERFACE void LoggingSystem_RegisterLoggingListener( ILoggingListener *pListener )
{
	// Single-listener stand-in -- last one registered wins. Real Valve
	// code chains multiple listeners; not needed for our purposes yet.
	g_pListener = pListener;
}

PLATFORM_INTERFACE void LoggingSystem_ResetCurrentLoggingState()
{
	// No-op: we don't track per-thread/global "current state" separately
	// from the channel table itself.
}

PLATFORM_INTERFACE bool LoggingSystem_IsChannelEnabled( LoggingChannelID_t channelID, LoggingSeverity_t severity )
{
	if ( channelID < 0 || channelID >= g_nChannelCount )
		return false;

	return severity >= g_Channels[channelID].m_MinSeverity;
}

PLATFORM_INTERFACE void LoggingSystem_SetChannelSpewLevel( LoggingChannelID_t channelID, LoggingSeverity_t minimumSeverity )
{
	if ( channelID < 0 || channelID >= g_nChannelCount )
		return;

	g_Channels[channelID].m_MinSeverity = minimumSeverity;
}

PLATFORM_INTERFACE LoggingChannelID_t LoggingSystem_FindChannel( const char *pChannelName )
{
	for ( int i = 0; i < g_nChannelCount; ++i )
	{
		if ( strcmp( g_Channels[i].m_szName, pChannelName ) == 0 )
			return i;
	}
	return INVALID_LOGGING_CHANNEL_ID;
}

PLATFORM_INTERFACE int LoggingSystem_GetChannelCount()
{
	return g_nChannelCount;
}

PLATFORM_INTERFACE LoggingResponse_t LoggingSystem_LogDirect( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessage )
{
	const char *pChannelName = ( channelID >= 0 && channelID < g_nChannelCount ) ? g_Channels[channelID].m_szName : "Unknown";

	if ( g_pListener )
	{
		LoggingContext_t ctx;
		ctx.m_ChannelID = channelID;
		ctx.m_Flags = ( channelID >= 0 && channelID < g_nChannelCount ) ? g_Channels[channelID].m_Flags : 0;
		ctx.m_Severity = severity;
		ctx.m_Color = spewColor;
		g_pListener->Log( &ctx, pMessage );
	}
	else
	{
		printf( "%s[%s] %s\033[0m", ColorToAnsi( spewColor ), pChannelName, pMessage );
	}

	return LR_CONTINUE;
}

PLATFORM_INTERFACE LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, const char *pMessageFormat, ... )
{
	char szBuf[2048];
	va_list args;
	va_start( args, pMessageFormat );
	vsnprintf( szBuf, sizeof( szBuf ), pMessageFormat, args );
	va_end( args );

	Color defaultColor = ( channelID >= 0 && channelID < g_nChannelCount ) ? g_Channels[channelID].m_Color : Color( 255, 255, 255, 255 );
	return LoggingSystem_LogDirect( channelID, severity, defaultColor, szBuf );
}

PLATFORM_OVERLOAD LoggingResponse_t LoggingSystem_Log( LoggingChannelID_t channelID, LoggingSeverity_t severity, Color spewColor, const char *pMessageFormat, ... )
{
	char szBuf[2048];
	va_list args;
	va_start( args, pMessageFormat );
	vsnprintf( szBuf, sizeof( szBuf ), pMessageFormat, args );
	va_end( args );

	return LoggingSystem_LogDirect( channelID, severity, spewColor, szBuf );
}

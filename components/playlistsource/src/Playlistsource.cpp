/*
//
// BEGIN SONGBIRD GPL
// 
// This file is part of the Songbird web player.
//
// Copyright� 2006 Pioneers of the Inevitable LLC
// http://songbirdnest.com
// 
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the �GPL�).
// 
// Software distributed under the License is distributed 
// on an �AS IS� basis, WITHOUT WARRANTY OF ANY KIND, either 
// express or implied. See the GPL for the specific language 
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this 
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc., 
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
// 
// END SONGBIRD GPL
//
 */

/** 
* \file  Playlistsource.cpp
* \brief Songbird Playlistsource Component Implementation.
*/

#define WINDOWS_LEAN_AND_MEAN
#include <Windows.h>

#include "nscore.h"
#include "prlog.h"

#include "nspr.h"
#include "nsCOMPtr.h"
#include "rdf.h"

#include "nsIEnumerator.h"
#include "nsIRDFService.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIInterfaceRequestor.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIContent.h"
#include "nsIXULTemplateBuilder.h"
#include "nsITimer.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsEnumeratorUtils.h"

#include <xpcom/nsXPCOM.h>
#include <xpcom/nsIFile.h>
#include <xpcom/nsILocalFile.h>
#include <xpcom/nsAutoLock.h>
/*
#include "nsIRDFFileSystem.h"
#include "nsRDFBuiltInPlaylistsources.h"
*/

#include "nsCRT.h"
#include "nsRDFCID.h"
#include "nsLiteralString.h"

#include "Playlistsource.h"

#include "IPlaylist.h"

#define OUTPUT_DEBUG 0
#define USE_TIMING 0

#if OUTPUT_DEBUG
#define SAYW( s ) ::OutputDebugStringW( s )
#define SAYA( s ) ::OutputDebugStringA( s )
#define SAY( s ) ::OutputDebugString( s )
#else
#define SAYW( s ) // ::OutputDebugStringW( s )
#define SAYA( s ) // ::OutputDebugStringA( s )
#define SAY( s ) // ::OutputDebugString( s )
#endif

static  sbPlaylistsource  *gPlaylistPlaylistsource = nsnull;
static  nsIRDFService       *gRDFService = nsnull;

// A callback from the database for when things change and we should repaint.
class MyQueryCallback : public sbIDatabaseSimpleQueryCallback
{
  NS_DECL_ISUPPORTS
  NS_DECL_SBIDATABASESIMPLEQUERYCALLBACK

public:
  sbPlaylistsource::sbFeedInfo  *m_Info;

  MyQueryCallback()
  {
    m_Timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  static void MyTimerCallbackFunc(nsITimer *aTimer, void *aClosure)
  {
    if ( gPlaylistPlaylistsource )
    {
      gPlaylistPlaylistsource->UpdateObservers();
    }
  }
  nsCOMPtr< nsITimer >          m_Timer;
};
NS_IMPL_ISUPPORTS1(MyQueryCallback, sbIDatabaseSimpleQueryCallback)
/* void OnQueryEnd (in sbIDatabaseResult dbResultObject, in wstring dbGUID, in wstring strQuery); */
NS_IMETHODIMP MyQueryCallback::OnQueryEnd(sbIDatabaseResult *dbResultObject, const PRUnichar *dbGUID, const PRUnichar *strQuery)
{
  // LOCK IT.
  SAYA("Callback Lock\n");
  nsAutoMonitor mon(sbPlaylistsource::g_pMonitor);
  SAYA("Callback Granted\n");

  // Push the old resultset onto the garbage stack.
  sbPlaylistsource::sbResultInfo result;
  result.m_Results = m_Info->m_Resultset;
  result.m_Source = m_Info->m_RootResource;
  result.m_OldTarget = m_Info->m_RootTargets;
  result.m_Ref = m_Info->m_Ref;
  result.m_ForceGetTargets = m_Info->m_ForceGetTargets;
  sbPlaylistsource::g_ResultGarbage.push_back( result );

  // Orphan the result for the query.
  sbIDatabaseResult *res; //, *resultset;
  m_Info->m_Query->GetResultObjectOrphan( &res );
  //resultset = res;
  //res->Release(); // ah-hah!
  m_Info->m_Resultset = res;

#if OUTPUT_DEBUG
  {
    PRUnichar str[255];
    wsprintfW( str, L"Callback - param(%08X) - query(%08X)\n", dbResultObject, resultset );
    SAYW( str );
  }
#endif

  if ( --sbPlaylistsource::g_ActiveQueryCount <= 0 )
  {
    sbPlaylistsource::g_NeedUpdate = PR_TRUE;
    sbPlaylistsource::g_ActiveQueryCount = 0;

    // Tell us to wake up in the main thread so we can poke our
    // observers and take out our garbage.
    if ( m_Timer.get() )
    {
      m_Timer->Cancel();
    }
    m_Timer->InitWithFuncCallback( &MyTimerCallbackFunc, gPlaylistPlaylistsource, 0, 0 );
  }


#if defined(WIN32) && 1 // OUTPUT_DEBUG  // yah, okay, now we shouldn't be naughty with win32 in the code anymore.
  static PRBool bOnce = PR_FALSE;

  PRUnichar *query_str = nsnull;
  m_Info->m_Query->GetQuery( 0, &query_str );
  {
    static PRUnichar txt[2048];
    PRInt32 num_rows, db_rows;
    m_Info->m_Resultset->GetRowCount( &num_rows );
    dbResultObject->GetRowCount( &db_rows );
    wsprintfW( txt, L"%s - `%s`\nResults has %d row%c and %d dbrow%c\n\n", dbGUID, query_str, num_rows, (num_rows == 1) ? ' ' : 's' , db_rows, (db_rows == 1) ? ' ' : 's' );
    OutputDebugStringW( txt );

    if( num_rows && !bOnce )
    {
      bOnce = PR_TRUE;
    }

    if ( !num_rows && bOnce)
    {

//      __asm PRInt32 3;
    }
  }
  PR_Free( query_str );
#endif

  SAYA("Callback Unlock\n");
  SAYA("Callback Exit\n");
  return NS_OK;
}

PRInt32 sbPlaylistsource::gRefCnt;

nsIRDFResource      *sbPlaylistsource::kNC_Playlist;

// nsIRDFResource      *sbPlaylistsource::kNC_Metadata[ sbPlaylistsource::kNumMetadataColumns ];

nsIRDFResource      *sbPlaylistsource::kNC_child;
nsIRDFResource      *sbPlaylistsource::kNC_pulse;

nsIRDFResource      *sbPlaylistsource::kRDF_InstanceOf;
nsIRDFResource      *sbPlaylistsource::kRDF_type;
nsIRDFResource      *sbPlaylistsource::kRDF_nextVal;

nsIRDFResource      *sbPlaylistsource::kRDF_Seq;

nsIRDFLiteral       *sbPlaylistsource::kLiteralTrue;
nsIRDFLiteral       *sbPlaylistsource::kLiteralFalse;

sbPlaylistsource::observers_t   sbPlaylistsource::g_Observers;
sbPlaylistsource::stringmap_t   sbPlaylistsource::g_StringMap;
sbPlaylistsource::infomap_t     sbPlaylistsource::g_InfoMap;
sbPlaylistsource::valuemap_t    sbPlaylistsource::g_ValueMap;
sbPlaylistsource::resultlist_t  sbPlaylistsource::g_ResultGarbage;
sbPlaylistsource::commandmap_t  sbPlaylistsource::g_CommandMap;
PRMonitor*                      sbPlaylistsource::g_pMonitor = nsnull;
PRInt32                             sbPlaylistsource::g_ActiveQueryCount = 0;
PRBool                            sbPlaylistsource::g_NeedUpdate = PR_FALSE;
nsCOMPtr< nsIStringBundle >     sbPlaylistsource::m_StringBundle;

// Strings that we'll use to compose all the goofy queries.
static nsString select_str( NS_LITERAL_STRING( "select " ) );
static nsString unique_str( NS_LITERAL_STRING( "select distinct" ) );
static nsString row_str( NS_LITERAL_STRING( "id" ) );
static nsString rowid_str( NS_LITERAL_STRING( "_ROWID_" ));
static nsString from_str( NS_LITERAL_STRING( " from " ) );
static nsString where_str( NS_LITERAL_STRING( " where " ) );
static nsString having_str( NS_LITERAL_STRING( " having " ) );
static nsString like_str( NS_LITERAL_STRING( " like " ) );
static nsString order_str( NS_LITERAL_STRING( " order by " ) );
static nsString left_join_str( NS_LITERAL_STRING( " left join " ) );
static nsString right_join_str( NS_LITERAL_STRING( " right join " ) );
static nsString inner_join_str( NS_LITERAL_STRING( " inner join " ) );
static nsString collate_binary_str( NS_LITERAL_STRING( " collate binary " ) );
static nsString on_str( NS_LITERAL_STRING( " on " ) );
static nsString using_str( NS_LITERAL_STRING( " using" ) );
static nsString limit_str( NS_LITERAL_STRING( " limit " ) );
static nsString op_str( NS_LITERAL_STRING( "(" ) );
static nsString cp_str( NS_LITERAL_STRING( ")" ) );
static nsString eq_str( NS_LITERAL_STRING( "=" ) );
static nsString ge_str( NS_LITERAL_STRING( ">=" ) );
static nsString qu_str( NS_LITERAL_STRING( "\"" ) );
static nsString pct_str( NS_LITERAL_STRING( "%" ) );
static nsString dot_str( NS_LITERAL_STRING( "." ) );
static nsString comma_str( NS_LITERAL_STRING( "," ));
static nsString spc_str( NS_LITERAL_STRING( " " ) );
static nsString and_str( NS_LITERAL_STRING( " and " ) );
static nsString or_str( NS_LITERAL_STRING( " or " ) );
static nsString uuid_str( NS_LITERAL_STRING( "uuid" ) );
static nsString playlist_uuid_str( NS_LITERAL_STRING( "playlist_uuid" ) );
static nsString library_str( NS_LITERAL_STRING( "library" ) );

// CLASSES ====================================================================
NS_IMPL_ISUPPORTS2(sbPlaylistsource, sbIPlaylistsource, nsIRDFDataSource)
//-----------------------------------------------------------------------------
sbPlaylistsource::sbPlaylistsource()
{
  g_pMonitor = nsAutoMonitor::NewMonitor("sbPlaylistsource.g_pMonitor");
  NS_ASSERTION(g_pMonitor, "sbPlaylistsource.g_pMonitor failed");
  Init();
} //ctor

//-----------------------------------------------------------------------------
/*virtual*/ sbPlaylistsource::~sbPlaylistsource()
{
  DeInit();
  if (g_pMonitor)
    nsAutoMonitor::DestroyMonitor(g_pMonitor);
} //dtor

NS_IMETHODIMP sbPlaylistsource::FeedPlaylist(const PRUnichar *RefName, const PRUnichar *ContextGUID, const PRUnichar *TableName)
{
  if ( ! ( RefName && ContextGUID && TableName ) )
  {
    return NS_OK;
  }

  PRInt32 Execute = 0; // legacy crap

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  // Find the ref string in the stringmap.
  nsString strRefName( RefName );

  // See if the feed is already there.
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    // Found it.  Get the refcount and increment it.
    info->m_RefCount++;
    info->m_Resultset = nsnull;
/*
    if ( Execute )
    {
      PRInt32 ret;
      info->m_Query->Execute( &ret );
    }
*/
  }
  else
  {
    // A new feed.  Fire a new Query object and stuff it in the tables.
    nsCOMPtr< sbIDatabaseQuery > query = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
    if ( query.get() )
    {
      query->SetAsyncQuery( PR_TRUE );
      query->SetDatabaseGUID( ContextGUID );

      // New callback for a new query.
      MyQueryCallback *callback = new MyQueryCallback;
      callback->AddRef();
      query->AddSimpleQueryCallback( callback );
      query->SetPersistentQuery( PR_TRUE );

      // Fire it.
      //if ( Execute )
      //{
      //  // Load it.
      //  nsString query_str( NS_LITERAL_STRING( "select * from " ) );
      //  nsString table_name( TableName );

      //  query_str += qu_str + table_name + qu_str;

      //  if(table_name != library_str)
      //  {
      //    nsCOMPtr<sbISimplePlaylist> pPlaylist;
      //    nsCOMPtr<sbIDatabaseQuery> pQuery = do_CreateInstance("@songbird.org/Songbird/DatabaseQuery;1");
      //    nsCOMPtr<sbIPlaylistManager> pPlaylistManager = do_CreateInstance("@songbird.org/Songbird/PlaylistManager;1");

      //    pQuery->SetAsyncQuery(PR_TRUE);
      //    pQuery->SetDatabaseGUID(ContextGUID);

      //    g_Lock.Unlock();
      //    pPlaylistManager->GetSimplePlaylist(table_name.get(), pQuery.get(), pPlaylist.StartAssignment());
      //    g_Lock.Lock();

      //    if(!pPlaylist.get())
      //    {
      //      query_str += left_join_str + library_str + on_str + qu_str + table_name + qu_str + dot_str + playlist_uuid_str + eq_str + library_str + dot_str + uuid_str;
      //      //        ::MessageBoxW( nsnull, query_str.get(), query_str.get(), MB_OK );
      //    }
      //  }

      //  query->AddQuery( query_str.get() );

      //  PRInt32 ret;
      //  query->Execute( &ret );
      //}

      // Make a resource for it.
      nsIRDFResource *new_resource = nsnull;
      gRDFService->GetResource( NS_ConvertUTF16toUTF8(strRefName), &new_resource );

      // And stuff it.
      sbFeedInfo info;
      info.m_Query = query;
      info.m_RefCount = 1;
      info.m_Ref = strRefName;
      info.m_Table = TableName;
      info.m_GUID = ContextGUID;
      info.m_RootResource = new_resource;
      info.m_Server = this;
      info.m_Callback = callback;
      info.m_ForceGetTargets = false;
      g_InfoMap[ new_resource ] = info;
      callback->m_Info = &g_InfoMap[ new_resource ];
      g_StringMap[ strRefName ] = new_resource;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::ClearPlaylist(const PRUnichar *RefName)
{
  ClearPlaylistSTR( RefName );
  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::IncomingObserver(const PRUnichar *RefName, nsIDOMNode *Observer)
{
  PRBool found = PR_FALSE;
  for ( observers_t::iterator oi = g_Observers.begin(); oi != g_Observers.end(); oi++ )
  {
    // Find the pointer?
    if ( (*oi).m_Ptr == Observer )
    {
      (*oi).m_Ref = RefName;
      found = PR_TRUE;
      // Assume that no AddObserver call will occur.
    }
  }
  if ( !found )
  {
    // This better be blank.
    if ( m_IncomingObserver.Length() )
    {
      MessageBoxW( nsnull, m_IncomingObserver.get(), RefName, MB_OK );
    }
    m_IncomingObserver = RefName;
    m_IncomingObserverPtr = Observer;
  }
  return NS_OK;
}

void sbPlaylistsource::ClearPlaylistSTR(const PRUnichar *RefName)
{
  if ( ! ( RefName ) )
  {
    return;
  }

  // LOCK IT.
  //nsAutoMonitor mon(g_pMonitor);

  // Find the ref string in the stringmap.
  nsString strRefName( RefName );
  stringmap_t::iterator s = g_StringMap.find( strRefName );
  if ( s != g_StringMap.end() )
  {
    ClearPlaylistRDF( (*s).second );
  }
}

void sbPlaylistsource::ClearPlaylistRDF(nsIRDFResource *RefResource)
{
  // LOCK IT.
  //nsAutoMonitor mon(g_pMonitor);

  sbFeedInfo *info = GetFeedInfo( RefResource );
  if ( info )
  {
    // Found it.  Get the refcount and decrement it.
    if ( --info->m_RefCount == 0 )
    {
      EraseFeedInfo( RefResource );
    }
  }
}

NS_IMETHODIMP sbPlaylistsource::GetQueryResult(const PRUnichar *RefName, sbIDatabaseResult **_retval)
{
  *_retval = nsnull;

  if ( RefName )
  {
    // Find the ref string in the stringmap.
    nsString strRefName( RefName );
    sbFeedInfo *info = GetFeedInfo( strRefName );
    if ( info )
    {
      // LOCK IT.
      nsAutoMonitor mon(g_pMonitor);
      if ( info->m_Resultset )
      {
        *_retval = info->m_Resultset;
        (*_retval)->AddRef();
      }
      else
      {
        info->m_Query->GetResultObject( _retval );
      }
    }
  }

  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::GetRefRowCount(const PRUnichar *RefName, PRInt32 *_retval)
{
  *_retval = -1;

  if ( RefName )
  {
    sbIDatabaseResult *resultset = nsnull;
    GetQueryResult( RefName, &resultset );
    if ( resultset )
    {
      resultset->GetRowCount( _retval );
      resultset->Release();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::GetRefColumnCount(const PRUnichar *RefName, PRInt32 *_retval)
{
  *_retval = -1;

  if ( RefName )
  {
    sbIDatabaseResult *resultset = nsnull;
    GetQueryResult( RefName, &resultset );
    if ( resultset )
    {
      resultset->GetColumnCount( _retval );
      resultset->Release();
    }
  }
  return NS_OK;
}

/* wstring GetRefRowCellByColum (in wstring RefName, in PRInt32 Row, in wstring Column); */
NS_IMETHODIMP sbPlaylistsource::GetRefRowCellByColumn(const PRUnichar *RefName, PRInt32 Row, const PRUnichar *Column, PRUnichar **_retval)
{
  nsAutoMonitor mon(g_pMonitor);
  if ( RefName )
  {
    // Find the ref string in the stringmap.
    nsString strRefName( RefName );
    sbFeedInfo *info = GetFeedInfo( strRefName );
    if ( info )
    {
      nsIRDFResource *next_resource = info->m_ResList[ Row ];
      valuemap_t::iterator v = g_ValueMap.find( next_resource );
      if ( v != g_ValueMap.end() )
      {
        if ( ! (*v).second.m_Resultset.get() )
        {
          LoadRowResults( (*v).second );
        }
        (*v).second.m_Resultset->GetRowCellByColumn( (*v).second.m_ResultsRow, Column, _retval );
      }
    }
  }
  return NS_OK;
}


/* PRInt32 GetRefRowByColumnValue (in wstring RefName, in PRInt32 Column, in wstring Value); */
NS_IMETHODIMP sbPlaylistsource::GetRefRowByColumnValue(const PRUnichar *RefName, const PRUnichar * Column, const PRUnichar *Value, PRInt32 *_retval)
{
  nsAutoMonitor mon(g_pMonitor);
  *_retval = -1;
  if ( RefName )
  {
    // Find the ref string in the stringmap.
    nsString strRefName( RefName );
    sbFeedInfo *info = GetFeedInfo( strRefName );
    if ( info )
    {
      // Steal the query info used for the ref
      m_SharedQuery->ResetQuery();
      PRUnichar *g = nsnull;
      info->m_Query->GetDatabaseGUID( &g );
      nsString guid( g );
      m_SharedQuery->SetDatabaseGUID( guid.get() );
      PRUnichar *oq = nsnull;
      info->m_Query->GetQuery( 0, &oq );
      nsString q( oq );

      // If we already have a where, don't add another one
      nsString aw_str;
      if ( q.Find( "where", PR_TRUE ) == -1 )
      {
        aw_str = NS_LITERAL_STRING( " where " );
      }
      else
      {
        aw_str = NS_LITERAL_STRING( " and " );
      }

      // Append the metadata column restraint
      nsString value_str( Value );
      nsString column_str( Column );
      q += aw_str + column_str + eq_str + qu_str + value_str + qu_str;

      PRInt32 exe;
      m_SharedQuery->AddQuery( q.get() );
      mon.Exit();
      m_SharedQuery->Execute( &exe );
      mon.Enter();

      nsCOMPtr< sbIDatabaseResult > result;
      m_SharedQuery->GetResultObject( getter_AddRefs( result ) );
      PRUnichar *val = nsnull;
      result->GetRowCell( 0, 0, &val );
      if ( val )
      {
        nsString v( val );

        // (sigh) Now linear search the info results object for the matching id value to get the result index
        PRInt32 rowcount;
        info->m_Resultset->GetRowCount( &rowcount );
        PRInt32 i = 0;
        for ( ; i < rowcount; i++ )
        {
          PRUnichar *val = nsnull;
          info->m_Resultset->GetRowCell( i, 0, &val );
          if ( v == nsString( val ) )
          {
            PR_Free( val );
            break;
          }
          PR_Free( val );
        }
        if ( i == rowcount )
        {
          i = -1;
        }

        *_retval = i;
        PR_Free( val );
      }
    }
  }
  return NS_OK;
}

/* PRBool IsQueryExecuting (in wstring RefName); */
NS_IMETHODIMP sbPlaylistsource::IsQueryExecuting(const PRUnichar *RefName, PRBool *_retval)
{
  *_retval = PR_FALSE;

  if ( RefName )
  {
    // Find the ref string in the stringmap.
    nsString strRefName( RefName );
    sbFeedInfo *info = GetFeedInfo( strRefName );
    if ( info )
    {
      info->m_Query->IsExecuting( _retval );
    }
  }
  return NS_OK;
}

/* void FeedPlaylistFilterOverride (in wstring RefName, in wstring FilterString); */
NS_IMETHODIMP sbPlaylistsource::FeedPlaylistFilterOverride(const PRUnichar *RefName, const PRUnichar *FilterString)
{
  if ( ! ( RefName && FilterString ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  nsString filter_str( FilterString );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info && info->m_Resultset.get() )
  {
    info->m_Override = filter_str;
    if ( filter_str.Length() )
    {
      // Crack the incoming list of filter strings (space delimited)
      std::vector< nsString > filter_values;
      nsString::const_iterator start, end;
      filter_str.BeginReading( start );
      filter_str.EndReading( end );
      for ( PRInt32 sub_start = 0, count = 0; start != end; start++, count++ )
      {
        if ( *start == ' ' )
        {
          if ( count - sub_start )
          {
            const nsDependentSubstring sb = Substring( filter_str, sub_start, count - sub_start );
            nsString sub;
            sub += sb;
            filter_values.push_back( sub );
          }
          sub_start = count + 1;
        }
      }
      if ( count - sub_start )
      {
        const nsDependentSubstring sb = Substring( filter_str, sub_start, count - sub_start );
        nsString sub;
        sub += sb;
        filter_values.push_back( sub );
        sub_start = count + 1;
      }

      nsString table_name( info->m_Table );

      // The beginning of the main query string before we dump on it.
      nsString main_query_str = info->m_SimpleQueryStr + where_str; // + op_str;
      // Compose an override query from the filter string and the columns in the current results
      PRInt32 col_count, filter_count = (PRInt32)info->m_Filters.size();
      info->m_Resultset->GetColumnCount( &col_count );
      PRBool any_column = PR_FALSE;
      if ( filter_count )
      {
        // We're going to submit n+1 queries;
        g_ActiveQueryCount += filter_count + 1;

        for ( filtermap_t::iterator f = info->m_Filters.begin(); f != info->m_Filters.end(); f++ )
        {
          // Compose an override string for the filter query.
          nsString sub_query_str = unique_str + op_str + (*f).second.m_Column + cp_str + from_str + qu_str + table_name + qu_str + where_str + op_str;
          PRBool any_value = PR_FALSE;
          for ( std::vector< nsString >::iterator str = filter_values.begin(); str != filter_values.end(); str++ )
          {
            if ( any_value )
            {
              sub_query_str += and_str;
            }
            any_value = PR_TRUE;
            sub_query_str += op_str + (*f).second.m_Column + like_str + qu_str + pct_str + (*str) + pct_str + qu_str + cp_str;
          }
          sub_query_str += cp_str + order_str + (*f).second.m_Column;

          sbFeedInfo *filter_info = GetFeedInfo( (*f).second.m_Ref );
          if ( filter_info )
          {
            PRInt32 ret;
            filter_info->m_Override = filter_str;
            filter_info->m_Query->ResetQuery();
            filter_info->m_Query->AddQuery( sub_query_str.get() );
//            ::MessageBoxW( nsnull, sub_query_str.get(), sub_query_str.get(), MB_OK );
            filter_info->m_Query->Execute( &ret );
          }
          else
          {
//            __asm PRInt32 3;
          }
        }

        PRBool any_value = PR_FALSE;
        for ( std::vector< nsString >::iterator str = filter_values.begin(); str != filter_values.end(); str++ )
        {
          if ( any_value )
          {
            main_query_str += and_str;
          }
          any_value = PR_TRUE;
          any_column = PR_FALSE;
          main_query_str += op_str;

          for ( f = info->m_Filters.begin(); f != info->m_Filters.end(); f++ )
          {
            // Add to the main query override string
            if ( any_column )
            {
              main_query_str += or_str;
            }
            any_column = PR_TRUE;
            main_query_str += op_str + (*f).second.m_Column + like_str + qu_str + pct_str + (*str) + pct_str + qu_str + cp_str;
          }

          main_query_str += or_str + op_str + NS_LITERAL_STRING("title") + like_str + qu_str + pct_str + (*str) + pct_str + qu_str + cp_str;
          main_query_str += cp_str;
        }
      }
      else if ( col_count ) // do it from the actual columns if there's no filters.
      {
        PRBool any_value = PR_FALSE;
        for ( std::vector< nsString >::iterator str = filter_values.begin(); str != filter_values.end(); str++ )
        {
          if ( any_value )
          {
            main_query_str += and_str;
          }
          else
          {
            main_query_str += op_str;
          }
          any_value = PR_TRUE;
          any_column = PR_FALSE;
/*
          for ( PRInt32 i = 0; i < col_count; i++ )
          {
            if ( any_column )
            {
              main_query_str += or_str;
            }
            else
            {
              main_query_str += op_str;
            }
            any_column = PR_TRUE;
            PRUnichar *col_name = nsnull;
            info->m_Resultset->GetColumnName( i, &col_name );
            nsString col_str( col_name );
            main_query_str += col_str + like_str + qu_str + pct_str + (*str) + pct_str + qu_str;
            PR_Free( col_name );
          }
          if ( any_column )
          {
            main_query_str += cp_str;
          }
          else
*/
          {
            main_query_str += op_str + NS_LITERAL_STRING("title") + like_str + qu_str + pct_str + (*str) + pct_str + qu_str;
            main_query_str += or_str + NS_LITERAL_STRING("genre") + like_str + qu_str + pct_str + (*str) + pct_str + qu_str;
            main_query_str += or_str + NS_LITERAL_STRING("artist") + like_str + qu_str + pct_str + (*str) + pct_str + qu_str;
            main_query_str += or_str + NS_LITERAL_STRING("album") + like_str + qu_str + pct_str + (*str) + pct_str + qu_str;
            main_query_str += cp_str;
          }
        }
        if ( any_value )
        {
          main_query_str += cp_str;
        }
      }
//      main_query_str += cp_str;

      mon.Exit();

      PRInt32 ret;
//      ::MessageBoxW( nsnull, main_query_str.get(), main_query_str.get(), MB_OK );
      info->m_Query->ResetQuery();
      info->m_Query->AddQuery( main_query_str.get() );
      info->m_Query->Execute( &ret );
    }
    else
    {
      // Revert to the normal override.
      mon.Exit();
      FeedFilters( RefName, nsnull );
    }
  }

  return NS_OK;
}

/* wstring GetFilterOverride (in wstring RefName); */
NS_IMETHODIMP sbPlaylistsource::GetFilterOverride(const PRUnichar *RefName, PRUnichar **_retval)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  PRBool set = PR_FALSE;
  if ( info )
  {
    if ( info->m_Override.Length() )
    {
      // Clunky, return the string
      PRUint32 nLen = info->m_Override.Length() + 1;
      *_retval = (PRUnichar *)PR_Calloc(nLen, sizeof(PRUnichar));
      memcpy(*_retval, info->m_Override.get(), (nLen - 1) * sizeof(PRUnichar));
      (*_retval)[info->m_Override.Length()] = 0;
      set = PR_TRUE;
    }
  }

  if ( !set )
  {
    // "Uh... who?"
    *_retval = (PRUnichar *)PR_Calloc(1, sizeof(PRUnichar));
    **_retval = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::SetFilter(const PRUnichar *RefName, PRInt32 Index, const PRUnichar *FilterString, const PRUnichar *FilterRefName, const PRUnichar *FilterColumn)
{
  if ( ! ( RefName && FilterString && FilterRefName && FilterColumn ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    filtermap_t::iterator f = info->m_Filters.find( Index );
    if ( f != info->m_Filters.end() )
    {
      // Change the existing strings
      (*f).second.m_Filter = FilterString;
      (*f).second.m_Column = FilterColumn;
      (*f).second.m_Ref    = FilterRefName;
    }
    else
    {
      // Make a new one, add it to the playlist feed
      sbFilterInfo filter;
      filter.m_Filter = FilterString;
      filter.m_Column = FilterColumn;
      filter.m_Ref    = FilterRefName;
      info->m_Filters[ Index ] = filter;
    }
    // Set all the later filters blank.
    for ( f++; f != info->m_Filters.end(); f++ )
    {
      (*f).second.m_Filter = nsString();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::GetNumFilters(const PRUnichar *RefName, PRInt32 *_retval)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    *_retval = (PRInt32)info->m_Filters.size();
  }
  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::ClearFilter(const PRUnichar *RefName, PRInt32 Index)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    filtermap_t::iterator f = info->m_Filters.find( Index );
    if ( f != info->m_Filters.end() )
    {
      // Remember to make the destructor clean up
      info->m_Filters.erase( f );
    }
  }
  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::GetFilter(const PRUnichar *RefName, PRInt32 Index, PRUnichar **_retval)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);
  PRBool set = PR_FALSE;
  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    if ( ! info->m_Override.Length() )
    {
      filtermap_t::iterator f = info->m_Filters.find( Index );
      if ( f != info->m_Filters.end() )
      {
        // Clunky, return the string
        PRUint32 nLen = (*f).second.m_Filter.Length() + 1;
        *_retval = (PRUnichar *)PR_Calloc(nLen, sizeof(PRUnichar));
        memcpy(*_retval, (*f).second.m_Filter.get(), (nLen - 1) * sizeof(PRUnichar));
        (*_retval)[(*f).second.m_Filter.Length()] = 0;
        set = PR_TRUE;
      }
    }
  }
  if ( !set )
  {
    // If we have an override, pretend the filter string is blank.
    *_retval = (PRUnichar *)PR_Calloc(1, sizeof(PRUnichar));
    **_retval = 0;
  }
  return NS_OK;
}


NS_IMETHODIMP sbPlaylistsource::GetFilterRef(const PRUnichar *RefName, PRInt32 Index, PRUnichar **_retval)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);
  PRBool set = PR_FALSE;
  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );
  if ( info )
  {
    filtermap_t::iterator f = info->m_Filters.find( Index );
    if ( f != info->m_Filters.end() )
    {
      // Clunky, return the string
      PRUint32 nLen = (*f).second.m_Ref.Length() + 1;
      *_retval = (PRUnichar *)PR_Calloc(nLen, sizeof(PRUnichar));
      memcpy(*_retval, (*f).second.m_Ref.get(), (nLen - 1) * sizeof(PRUnichar));
      (*_retval)[(*f).second.m_Ref.Length()] = 0;
      set = PR_TRUE;
    }
  }
  if ( !set )
  {
    // If we have an override, pretend the ref string is blank.
    *_retval = (PRUnichar *)PR_Calloc(1, sizeof(PRUnichar));
    **_retval = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP sbPlaylistsource::FeedFilters(const PRUnichar *RefName, PRInt32 *_retval)
{
  if ( ! ( RefName ) )
  {
    return NS_OK;
  }

  // No, you don't HAVE to pass in a value.
  PRInt32 retval;
  if ( !_retval )
  {
    _retval = &retval;
  }

#if USE_TIMING
  FILETIME a_start, a_time;
  PRUnichar str[255];
  ::GetSystemTimeAsFileTime( &a_start );
  SAYA("FeedFilter - START\n" );
#endif

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );

  if ( info )
  {
    info->m_Override = nsString();
    nsString table_name( info->m_Table );

    nsCOMPtr<sbISimplePlaylist> pSimplePlaylist;
    nsCOMPtr<sbISmartPlaylist> pSmartPlaylist;
    nsCOMPtr<sbIDatabaseQuery> pQuery = do_CreateInstance("@songbird.org/Songbird/DatabaseQuery;1");
    nsCOMPtr<sbIPlaylistManager> pPlaylistManager = do_CreateInstance("@songbird.org/Songbird/PlaylistManager;1");

    pQuery->SetAsyncQuery(PR_TRUE);
    pQuery->SetDatabaseGUID(info->m_GUID.get());

    mon.Exit();
    pPlaylistManager->GetSimplePlaylist(table_name.get(), pQuery.get(), getter_AddRefs(pSimplePlaylist));
    pPlaylistManager->GetSmartPlaylist(table_name.get(), pQuery.get(), getter_AddRefs(pSmartPlaylist));
    mon.Enter();

    PRBool isSimplePlaylist = PR_FALSE;
    if(pSimplePlaylist.get()) isSimplePlaylist = PR_TRUE;

    PRBool isSmartPlaylist = PR_FALSE;
    if(pSmartPlaylist.get()) isSmartPlaylist = PR_TRUE;

    nsString library_query_str = select_str + row_str + from_str + qu_str + table_name + qu_str + where_str;
    nsString library_simple_query_str = select_str + row_str + from_str + qu_str + table_name + qu_str;

    // The beginning of the main query string before we dump on it.
    nsString playlist_simple_query_str = select_str + row_str + from_str + qu_str + table_name + qu_str + left_join_str + library_str + on_str +
      qu_str + table_name + qu_str + dot_str + playlist_uuid_str + eq_str + library_str + dot_str + uuid_str;

    nsString simple_query_str = ((table_name == library_str) || isSimplePlaylist || isSmartPlaylist) ? library_simple_query_str : playlist_simple_query_str;
    nsString main_query_str = simple_query_str + where_str;
    info->m_SimpleQueryStr = simple_query_str;
    // The filters for the filters.
    nsString sub_filter_str;

    // We're going to submit n+1 queries;
    g_ActiveQueryCount += (PRInt32)info->m_Filters.size() + 1;

    PRBool anything = PR_FALSE;
    for ( filtermap_t::iterator f = info->m_Filters.begin(); f != info->m_Filters.end(); f++ )
    {
      // Crack the incoming list of filter strings (semicolon delimited)
      nsString filter_str = (*f).second.m_Filter;
      std::vector< nsString > filter_values;
      nsString::const_iterator start, end;
      filter_str.BeginReading( start );
      filter_str.EndReading( end );
      for ( PRInt32 sub_start = 0, count = 0; start != end; start++, count++ )
      {
        if ( *start == ';' )
        {
          if ( count - sub_start )
          {
            const nsDependentSubstring sb = Substring( filter_str, sub_start, count - sub_start );
            nsString sub;
            sub += sb;
            filter_values.push_back( sub );
          }
          sub_start = count + 1;
        }
      }
      if ( count - sub_start )
      {
        const nsDependentSubstring sb = Substring( filter_str, sub_start, count - sub_start );
        nsString sub;
        sub += sb;
        filter_values.push_back( sub );
        sub_start = count + 1;
      }

      // Compose the SQL filter string
      nsString sql_filter_str;
      PRBool any_filter = PR_FALSE;
      for ( std::vector< nsString >::iterator s = filter_values.begin(); s != filter_values.end(); s++ )
      {
        if ( any_filter )
        {
          sql_filter_str += or_str;
        }
        sql_filter_str += (*f).second.m_Column + eq_str + qu_str + *s + qu_str;
        any_filter = PR_TRUE;
      }

      if ( any_filter )
      {
        // Append this filter's constraints to the main query
        if ( anything )
        {
          main_query_str += and_str;
        }
        main_query_str += op_str + sql_filter_str + cp_str;
      }

      // Compose the sub query
      nsString sub_query_str = unique_str + op_str + (*f).second.m_Column + cp_str + from_str + qu_str + table_name + qu_str;
      if ( anything )
      {
        sub_query_str += where_str + sub_filter_str;
      }
      sub_query_str += order_str + (*f).second.m_Column;

      // Append this filter's constraints to the next sub query
      if ( any_filter )
      {
        if ( anything )
        {
          sub_filter_str += and_str;
        }
        sub_filter_str += op_str + sql_filter_str + cp_str; 
      }

      // Make sure there is a feed for this filter
      sbFeedInfo *filter_info = GetFeedInfo( (*f).second.m_Ref );
      if ( !filter_info )
      {
        FeedPlaylist( (*f).second.m_Ref.get(), info->m_GUID.get(), info->m_Table.get() );
        filter_info = GetFeedInfo( (*f).second.m_Ref );
        if ( !filter_info )
        {
//          __asm PRInt32 3;
        }
      }

      // Remember the column
      filter_info->m_Column = (*f).second.m_Column;
      filter_info->m_Override = nsString();

      // Execute the new query for the feed
      PRBool bret;
      filter_info->m_Query->IsExecuting( &bret );
      if ( bret )
      {
        mon.Exit();
        SAYA("FeedFilter - ABORTING QUERY\n" );
        filter_info->m_Query->Abort( &bret ); // Blocks until aborted.
        SAYA("FeedFilter - ABORTED\n" );
        mon.Enter();
      }
      PRInt32 ret;
      filter_info->m_Query->ResetQuery();
      filter_info->m_Query->AddQuery( sub_query_str.get() );
//      g_Lock.Unlock();
      SAYA("FeedFilter - Execute - " );
#if USE_TIMING
      wsprintfW( str, L"(%08X): %s\n", filter_info->m_Query.get(), sub_query_str.get()  );
      SAYW( str );
#endif
      filter_info->m_Query->Execute( &ret );
//      g_Lock.Lock();

      // And note that at least one filter exists on the main query?  Maybe?
      if ( any_filter )
      {
        anything = PR_TRUE;
      }
    }

    // Only alter the main playlist query if we have current filters.
    if ( ! anything )
    {
      main_query_str = simple_query_str;
    }

    // It's highly probable that we're still executing from FeedPlaylist()
    PRBool bret;
    info->m_Query->IsExecuting( &bret );
    if ( bret )
    {
      SAYA("FeedFilter - ABORTING QUERY\n" );
/*
      g_Lock.Unlock();
      info->m_Query->Abort( &bret ); // Blocks until aborted.
      SAYA("FeedFilter - ABORTED\n" );
      g_Lock.Lock();
*/
    }
//    else
    {
      // Remove the previous results
      info->m_Resultset = nsnull;

      // Change the main query and resubmit.
      SAYA("FeedFilter - Reset\n" );
      info->m_Query->ResetQuery();
      //    ::MessageBoxW( nsnull, main_query_str.get(), main_query_str.get(), MB_OK );
      SAYA("FeedFilter - Add\n" );
      info->m_Query->AddQuery( main_query_str.get() );
      //    g_Lock.Unlock();
      SAYA("FeedFilter - Execute - " );
#if USE_TIMING
      wsprintfW( str, L"(%08X): %s\n", info->m_Query.get(), main_query_str.get()  );
      SAYW( str );
#endif
      info->m_Query->Execute( _retval );
      //    g_Lock.Lock();
    }
  }

#if USE_TIMING
  ::GetSystemTimeAsFileTime( &a_time );
  wsprintfW( str, L"FeedFilter - %06d\n", ( a_time.dwLowDateTime - a_start.dwLowDateTime ) / 10000 );
  SAYW( str );
#endif

  return NS_OK;
}

/* void RegisterPlaylistCommands (in wstring ContextGUID, in wstring TableName, in sbIPlaylistCommands CommandObj); */
NS_IMETHODIMP sbPlaylistsource::RegisterPlaylistCommands(const PRUnichar *ContextGUID, const PRUnichar *TableName, const PRUnichar *PlaylistType, sbIPlaylistCommands *CommandObj)
{
  if ( ! ( ContextGUID && TableName && CommandObj ) )
  {
    return NS_OK;
  }

  nsString key( ContextGUID );
  nsString type( PlaylistType );

  // Hah, uh, NO.
  if ( key == NS_LITERAL_STRING("songbird") )
  {
    return NS_OK;
  }

  key += TableName;

  g_CommandMap[ type ] = CommandObj;
  g_CommandMap[ key ] = CommandObj;

  return NS_OK;
}

/* sbIPlaylistCommands GetPlaylistCommands (in wstring ContextGUID, in wstring TableName); */
NS_IMETHODIMP sbPlaylistsource::GetPlaylistCommands(const PRUnichar *ContextGUID, const PRUnichar *TableName, const PRUnichar *PlaylistType, sbIPlaylistCommands **_retval)
{
  if ( ! ( ContextGUID && TableName  ) )
  {
    return NS_OK;
  }

  *_retval = nsnull;

  nsString key( ContextGUID );
  nsString type( PlaylistType );
  key += TableName;

  commandmap_t::iterator c = g_CommandMap.find( type );
  if ( c != g_CommandMap.end() )
  {
    (*c).second->Duplicate( _retval );
  }
  else
  {
    commandmap_t::iterator c = g_CommandMap.find( key );
    if ( c != g_CommandMap.end() )
    {
      (*c).second->Duplicate( _retval );
    }
  }
  return NS_OK;
}



void sbPlaylistsource::UpdateObservers()
{
  PRBool need_update = PR_FALSE;
  resultlist_t old_garbage;
  nsAutoMonitor mon(g_pMonitor);
  if ( g_NeedUpdate )
  {
    need_update = PR_TRUE;
    g_NeedUpdate = PR_FALSE;

    old_garbage = g_ResultGarbage; 
    g_ResultGarbage.clear();
  }
  mon.Exit();

  if ( need_update )
  {
    // Check to see if any are "forced" (loaded outside of the UI)
    for ( resultlist_t::iterator r = old_garbage.begin(); r != old_garbage.end(); r++ )
    {
      if ( (*r).m_ForceGetTargets )
      {
        ForceGetTargets( (*r).m_Ref.get() );
      }
    }

    // Inform the observers that everything changed.
    for ( sbPlaylistsource::observers_t::iterator o = sbPlaylistsource::g_Observers.begin(); o != sbPlaylistsource::g_Observers.end(); o++ )
    {
      nsString os = (*o).m_Ref;
      // Did the observer tell us which ref it wants to observe?
      if ( ! os.Length() )
      {
        // If not, always update it.
        (*o).m_Observer->OnBeginUpdateBatch( this );
        (*o).m_Observer->OnEndUpdateBatch( this );
      }
      else
      {
        // Otherwise, check to see does the observer match anything in the update list?
        for ( resultlist_t::iterator r = old_garbage.begin(); r != old_garbage.end(); r++ )
        {
          nsString rs = (*r).m_Ref;
          if ( rs == os  )
          {
            (*o).m_Observer->OnBeginUpdateBatch( this );
            (*o).m_Observer->OnEndUpdateBatch( this );
          }
        }
      }
    }

    // Only free the resultsets that observers might be using AFTER the observers have been told to wake up.
    for ( resultlist_t::iterator r = old_garbage.begin(); r != old_garbage.end(); r++ )
    {
      if ( (*r).m_Results.get() )
      {
#if USE_TIMING
        PRUnichar str[255];
        PRInt32 row, col;
        (*r).m_Results->GetRowCount( &row );
        (*r).m_Results->GetColumnCount( &col );
        wsprintfW( str, L"Garbage Collect - 0x%08X - %d, %d\n", (*r).m_Results.get(), row, col );
        SAYW( str );
#endif
        (*r).m_Results->ClearResultSet();
      }
    }
  }
}

void sbPlaylistsource::Init(void)
{
  if (gRefCnt++ == 0)
  {
    // Make the shared query
    m_SharedQuery = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
    m_SharedQuery->SetAsyncQuery( PR_FALSE ); 

    nsresult rv = NS_OK;

    // Get the string bundle for our strings
    if ( ! m_StringBundle.get() )
    {
      nsIStringBundleService *  StringBundleService = nsnull;
      rv = CallGetService("@mozilla.org/intl/stringbundle;1", &StringBundleService );
      if ( NS_SUCCEEDED(rv) )
      {
        rv = StringBundleService->CreateBundle( "chrome://Songbird/locale/songbird.properties", getter_AddRefs( m_StringBundle ) );
        //        StringBundleService->Release();
      }
    }

    if ( nsnull == gRDFService )
    {
      rv = CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
    }
    if ( NS_SUCCEEDED(rv) )
    {
      gRDFService->GetResource(NS_LITERAL_CSTRING("NC:Playlist"),
        &kNC_Playlist);

      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "child"),
        &kNC_child);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "pulse"),
        &kNC_pulse);

      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "instanceOf"),
        &kRDF_InstanceOf);
      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "type"),
        &kRDF_type);
      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "nextVal"),
        &kRDF_nextVal);
      //      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "li"),
      //        &kRDF_li);
      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Seq"),
        &kRDF_Seq);

      gRDFService->GetLiteral(NS_LITERAL_STRING("PR_TRUE").get(),       
        &kLiteralTrue);
      gRDFService->GetLiteral(NS_LITERAL_STRING("PR_FALSE").get(),      
        &kLiteralFalse);
    }
    else
    {
      SAYA("Init - No gRDFService, can't get resources.\n" );
    }

    gPlaylistPlaylistsource = this;
  }
}

void sbPlaylistsource::DeInit (void)
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - RDF: sbPlaylistsource\n", gInstanceCount);
#endif

  // DEINIT THE LOCAL 
/*
  std::map< nsIRDFResource *, PRInt32>::iterator i;
  for ( i = m_QueryResources.begin(); i != m_QueryResources.end(); i++ )
  {
    NS_RELEASE( const_cast<nsIRDFResource *>( (*i).first ) );
  }
  m_QueryResources.clear();
  ClearPlaylistRDF( m_RootResource );
  */

  // DEINIT THE STATIC 
  if (--gRefCnt == 0) 
  {
    if ( nsnull != gRDFService )
    {
      NS_RELEASE(kNC_Playlist);
/*
      PRInt32 i;
      for ( i = 0; i < sbPlaylistsource::kNumMetadataColumns; i++ )
      {
        NS_RELEASE(kNC_Metadata[ i ]);
      }
*/
      NS_RELEASE(kRDF_InstanceOf);
      NS_RELEASE(kRDF_type);
      NS_RELEASE(gRDFService);
    }
    gPlaylistPlaylistsource = nsnull;
  }
}

NS_IMETHODIMP
sbPlaylistsource::GetURI(char **uri)
{
  SAYA("GetURI\n" );
  NS_PRECONDITION(uri != nsnull, "null ptr");
  if (! uri)
    return NS_ERROR_NULL_POINTER;

  if ((*uri = nsCRT::strdup("rdf:playlist")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}



NS_IMETHODIMP
sbPlaylistsource::GetSource(nsIRDFResource* property,
                    nsIRDFNode* target,
                    PRBool tv,
                    nsIRDFResource** source /* out */)
{
  SAYA("GetSource\n" );
  NS_PRECONDITION(property != nsnull, "null ptr");
  if (! property)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(target != nsnull, "null ptr");
  if (! target)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  *source = nsnull;
  return NS_RDF_NO_VALUE;
}



NS_IMETHODIMP
sbPlaylistsource::GetSources(nsIRDFResource *property,
                     nsIRDFNode *target,
                     PRBool tv,
                     nsISimpleEnumerator **sources /* out */)
{
  SAYA("GetSources\n" );
  //  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
sbPlaylistsource::GetTarget(nsIRDFResource *source,
                    nsIRDFResource *property,
                    PRBool tv,
                    nsIRDFNode **target /* out */)
{
  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(property != nsnull, "null ptr");
  if (! property)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(target != nsnull, "null ptr");
  if (! target)
    return NS_ERROR_NULL_POINTER;

  SAYA("\nGetTarget\n" );

  nsCString value;
  source->GetValueUTF8( value );

#if OUTPUT_DEBUG
  const char *source_val = nsnull;
  source->GetValueConst( &source_val );
  SAYA("source: " );
  SAYA( source_val );
  const char *property_val = nsnull;
  property->GetValueConst( &property_val );
  SAYA("\nproperty: " );
  SAYA( property_val );
  SAYA("\n" );
#endif // OUTPUT_DEBUG

  *target = nsnull;

  nsresult        rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the file system data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  // LOCK IT.
  nsAutoMonitor mon(g_pMonitor);

  nsString outstring;
  // Look in the value map
  if ( ! outstring.Length() )
  {
    valuemap_t::iterator v = g_ValueMap.find( source );
    if ( v != g_ValueMap.end() )
    {
      // If we only have one value for this source ref, always return it.  
      // Magic special case for the filter lists.
      if ( (*v).second.m_Info->m_ColumnMap.size() == 1 )
      {
        PRUnichar *val = nsnull;
        if ( (*v).second.m_All )
        {
          outstring += NS_LITERAL_STRING("All");
        }
        else
        {
          (*v).second.m_Info->m_Resultset->GetRowCellPtr( (*v).second.m_Row, 0, &val );
        }
        outstring += val;
      }
      else
      {
        // Figure out the metadata column
        columnmap_t::iterator c = (*v).second.m_Info->m_ColumnMap.find( property );
        if ( c != (*v).second.m_Info->m_ColumnMap.end() )
        {
          // And return that.
          PRUnichar *val = nsnull;

          if ( property == (*v).second.m_Info->m_RowIdResource ) // row_id
          {
            outstring.AppendInt( (*v).second.m_Row + 1 );
          }
          else
          {
            if ( (*v).second.m_Info->m_Column.Length() == 0 )
            {
              // Only query the full metadata during get target.
              if ( (*v).second.m_Resultset.get() == nsnull )
              {
                LoadRowResults( (*v).second );
              }
              (*v).second.m_Resultset->GetRowCellPtr( (*v).second.m_ResultsRow, (*c).second, &val );
            }
            else
            {
              (*v).second.m_Info->m_Resultset->GetRowCellPtr( (*v).second.m_Row, (*c).second, &val );
            }
          }
          outstring += val;
//          PR_Free( val );    // GetRowCellPtr doesn't need freeing.
        }
      }
    }
  }

  if ( outstring.Length() )
  {
    // Recompose from the stringbundle
    if ( outstring[ 0 ] == '&' )
    {
      PRUnichar *key = (PRUnichar *)outstring.get() + 1;
      PRUnichar *value = nsnull;
      m_StringBundle->GetStringFromName( key, &value );
      if ( value && value[0] )
      {
        outstring = value;
      }
      PR_Free( value );
    }

    nsCOMPtr<nsIRDFLiteral> literal;
    rv = gRDFService->GetLiteral(outstring.get(), getter_AddRefs(literal));
    if (NS_FAILED(rv)) return(rv);
    if (!literal)   rv = NS_RDF_NO_VALUE;
    if (rv == NS_RDF_NO_VALUE) return(rv);

    return literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
  }

  return(NS_RDF_NO_VALUE);
}



NS_IMETHODIMP
sbPlaylistsource::ForceGetTargets(const PRUnichar *RefName)
{

  nsString strRefName( RefName );
  sbFeedInfo *info = GetFeedInfo( strRefName );

  if ( info )
  {
    // So, like, force it.
    nsCOMPtr< nsISimpleEnumerator > enumer;
    GetTargets( info->m_RootResource, kNC_child, true, getter_AddRefs( enumer ) );
    // And remember that it's forced.
    info->m_ForceGetTargets = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
sbPlaylistsource::GetTargets(nsIRDFResource *source,
                     nsIRDFResource *property,
                     PRBool tv,
                     nsISimpleEnumerator **targets /* out */)
{
  SAYA("\nGetTargets\n" );

#if OUTPUT_DEBUG
  const char *source_val = nsnull;
  source->GetValueConst( &source_val );
  SAYA("source: " );
  SAYA( source_val );
  const char *property_val = nsnull;
  property->GetValueConst( &property_val );
  SAYA("\nproperty: " );
  SAYA( property_val );
  SAYA("\n" );
#endif // OUTPUT_DEBUG

  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(property != nsnull, "null ptr");
  if (! property)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(targets != nsnull, "null ptr");
  if (! targets)
    return NS_ERROR_NULL_POINTER;

  *targets = nsnull;

  // we only have positive assertions in the file system data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  // We only respond targets to children, I guess.
  if (property == kNC_child)
  {

    PRInt32 rowcount = 0, colcount = 0;
#if USE_TIMING
    FILETIME a_start, a_time;
    PRUnichar str[255];
    ::GetSystemTimeAsFileTime( &a_start );
#endif

    // LOCK IT!
    nsAutoMonitor mon(g_pMonitor);

    // Okay, so, the "source" item should be found in the Map
    sbFeedInfo *info = GetFeedInfo( source );
    if ( info )
    {
      info->m_RefCount++; // Pleah.  This got away from me.
      if ( !info->m_Query.get() )
      {
        return NS_RDF_NO_VALUE;
      }

      // We need to create a new array enumerator, fill it with our next item, and send it back.
      nsresult rv = NS_OK;
      *targets = nsnull;

      // Make the array to hold our response
      nsCOMPtr<nsISupportsArray> nextItemArray;
      rv = NS_NewISupportsArray(getter_AddRefs(nextItemArray));
      if (NS_FAILED(rv))
      {
        return rv;
      }

      //
      // Let's try to reuse this so there's not so much map traffic?
      //

      // Clear out the current vector of column resources
      for ( columnmap_t::iterator c = info->m_ColumnMap.begin(); c != info->m_ColumnMap.end(); c++ )
      {
        NS_RELEASE( const_cast<nsIRDFResource *>( (*c).first ) );
      }
      info->m_ColumnMap.clear();

      // Using the resultset object, make a set of resources for the table data
      nsCOMPtr< sbIDatabaseResult > resultset;
      nsCOMPtr< sbIDatabaseResult > colresults; // Might be diff than resultset dep on #if

      if ( info->m_Resultset.get() )
      {
        resultset = info->m_Resultset.get();
      }
      else
      {
        sbIDatabaseResult *res;
        info->m_Query->GetResultObjectOrphan( &res );
        resultset = res;
        res->Release(); // ah-hah!
        info->m_Resultset = resultset;
      }

      resultset->GetRowCount( &rowcount );

      // If we're not a filterlist
      if ( info->m_Column.Length() == 0 )
      {
        // Get the first row from the table to know what the columns should be?

        // Set the guid from the info's query guid.
        PRInt32 exec = 0;
        m_SharedQuery->ResetQuery();
        PRUnichar *g = nsnull;
        info->m_Query->GetDatabaseGUID( &g );
        nsString guid( g );
        m_SharedQuery->SetDatabaseGUID( guid.get() );

        // Set the query string from the info's query string.
        PRUnichar *oq = nsnull;
        info->m_Query->GetQuery( 0, &oq );
        nsString q( oq );
        nsString tablerow_str = library_str + dot_str + row_str;
        nsString find_str = spc_str + tablerow_str + spc_str;
        if ( q.Find( find_str ) != -1 )
        {
          q.ReplaceSubstring( find_str, spc_str + NS_LITERAL_STRING("*,") + tablerow_str + spc_str );
        }
        else
        {
          q.ReplaceSubstring( NS_LITERAL_STRING(" id "), NS_LITERAL_STRING(" *,id ") );
        }
        q += NS_LITERAL_STRING( " limit 1" );
        m_SharedQuery->AddQuery( q.get() );

        // Then do it.
        mon.Exit();
        m_SharedQuery->Execute( &exec );
        mon.Enter();
        sbIDatabaseResult *res;
        m_SharedQuery->GetResultObjectOrphan( &res );
        colresults = res;
        res->Release(); // ah-hah!
        PR_Free( g );
        PR_Free( oq );
      }
      else
      {
        colresults = resultset;
      }

      // First re/create resources for the columns
      PRInt32 j;
      colresults->GetColumnCount( &colcount );
      for ( j = 0; j < colcount; j++ )
      {
        nsIRDFResource *col_resource = nsnull;
        PRUnichar *col_name;
        colresults->GetColumnName( j, &col_name );
        nsString col_str( col_name );
        nsCString utf8_resource_name = NS_LITERAL_CSTRING( NC_NAMESPACE_URI ) +
          NS_ConvertUTF16toUTF8(col_str);
        gRDFService->GetResource( utf8_resource_name, &col_resource );
        info->m_ColumnMap[ col_resource ] = j;
        PR_Free( col_name );
      }
      if ( !j )
      {
        // If there is a column for this item, use it instead.
        if ( info->m_Column.Length() )
        {
          nsIRDFResource *col_resource = nsnull;
          nsCString utf8_resource_name = NS_LITERAL_CSTRING( NC_NAMESPACE_URI ) +
            NS_ConvertUTF16toUTF8( info->m_Column );
          gRDFService->GetResource( utf8_resource_name, &col_resource );
          info->m_ColumnMap[ col_resource ] = 0;
          colcount = 1;
        }
      }
      else if ( j > 1 )
      {
        // Add the magic "row_id" column.
        nsIRDFResource *col_resource = nsnull;
        nsCString utf8_resource_name = NS_LITERAL_CSTRING( NC_NAMESPACE_URI "row_id" );
        gRDFService->GetResource( utf8_resource_name, &col_resource );
        info->m_ColumnMap[ col_resource ] = j;
        info->m_RowIdResource = col_resource;
      }

      PRInt32 size = (PRInt32)info->m_ResList.size();
      // One column must be a filter list.  (Well, for all practical purposes)
      PRInt32 start = 0;
      if ( colcount == 1 )
      {
        // Magic bonus row for the "All" selection.
        start ++;
        rowcount ++;

        // Make sure there's enough reslist items.
        if ( size < rowcount )
        {
          info->m_ResList.resize( rowcount );
          info->m_Values.resize( rowcount );
          // Insert nsnull.
          for ( PRInt32 i = size; i < rowcount; i++ )
          {
            info->m_ResList[ i ] = nsnull;
            info->m_Values[ i ] = nsnull;
          }
          size = (PRInt32)info->m_ResList.size();
        }

        // Grab the first item (or create it if it doesn't yet exist)
        nsIRDFResource *next_resource = info->m_ResList[ 0 ];
        if ( ! next_resource )
        {
          // Create a new resource for this item
          gRDFService->GetAnonymousResource( &next_resource );
          info->m_ResList[ 0 ] = next_resource;
        }

        // Place it in the retval
        nextItemArray->AppendElement( next_resource );

        // And create a map entry for this item.
        sbValueInfo value;
        g_ValueMap[ next_resource ] = value;
        g_ValueMap[ next_resource ].m_Info = info;
        g_ValueMap[ next_resource ].m_Row = 0;
        g_ValueMap[ next_resource ].m_All = PR_TRUE;
//        info->m_Values[ 0 ] = & g_ValueMap[ next_resource ];
      }

      // Make sure there's enough reslist items.
      if ( size < rowcount )
      {
        info->m_ResList.resize( rowcount );
        info->m_Values.resize( rowcount );
        // Insert nsnull (so we know later if we're recycling).
        for ( PRInt32 i = size; i < rowcount; i++ )
        {
          info->m_ResList[ i ] = nsnull;
          info->m_Values[ i ] = nsnull;
        }
      }

      // Store the new row resources in the array 
      // (and a map for quick lookup to the row PRInt32 -- STOOOOPID)

      for ( PRInt32 i = start; i < rowcount; i++ )
      {
        // Only create items if there is a value
        // (Safe because our schema does not allow this to be null for a real playlist)
        PRUnichar *ck;
        resultset->GetRowCellPtr( i - start, 0, &ck );
        nsString check( ck );
        if ( check.Length() || ( colcount > 1 ) )
        {
          nsIRDFResource *next_resource = info->m_ResList[ i ];
          if ( ! next_resource )
          {
            gRDFService->GetAnonymousResource( &next_resource ); // let's try anonymous ones, eh?
            info->m_ResList[ i ] = next_resource;

            sbValueInfo value;
            value.m_Info = info;
            value.m_Row = i - start;
            value.m_ResMapIndex = i;
            g_ValueMap[ next_resource ] = value;
          }
          // Always set these values
          sbValueInfo &value = g_ValueMap[ next_resource ];
          value.m_Id = check;
          value.m_Resultset = nsnull;

          nextItemArray->AppendElement( next_resource );

        }
//        PR_Free( ck );
      }

      // Stuff the array into the enumerator
      nsISimpleEnumerator* result = new nsArrayEnumerator(nextItemArray);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

      // And give it to whomever is calling us
      NS_ADDREF(result);
      *targets = result;
      info->m_RootTargets = result;

#if USE_TIMING
      ::GetSystemTimeAsFileTime( &a_time );
      wsprintfW( str, L"GetTargets - %06d (%d)\n", ( a_time.dwLowDateTime - a_start.dwLowDateTime ) / 10000, rowcount );
      SAYW( str );
#endif

      return rv;
    }
    else
    {
//      ::MessageBoxW( nsnull, L"bad source", L"sbPlaylistsource", MB_OK );
    }
  }

  return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
sbPlaylistsource::Assert(nsIRDFResource *source,
                 nsIRDFResource *property,
                 nsIRDFNode *target,
                 PRBool tv)
{
  SAYA("Assert\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
sbPlaylistsource::Unassert(nsIRDFResource *source,
                   nsIRDFResource *property,
                   nsIRDFNode *target)
{
  SAYA("Unassert\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
sbPlaylistsource::Change(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aOldTarget,
                 nsIRDFNode* aNewTarget)
{
  SAYA("Change\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
sbPlaylistsource::Move(nsIRDFResource* aOldSource,
               nsIRDFResource* aNewSource,
               nsIRDFResource* aProperty,
               nsIRDFNode* aTarget)
{
  SAYA("Move\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
sbPlaylistsource::HasAssertion(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv,
                       PRBool *hasAssertion /* out */)
{
  SAYA("\nHasAssertion\n" );

#if OUTPUT_DEBUG
  const char *source_val = nsnull;
  source->GetValueConst( &source_val );
  SAYA("source: " );
  SAYA( source_val );
  const char *property_val = nsnull;
  property->GetValueConst( &property_val );
  SAYA("\nproperty: " );
  SAYA( property_val );
  const char *target_val = nsnull;
  nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
  if ( resource.get() )
  {
    resource->GetValueConst( &target_val );
    SAYA("\ntarget: " );
    SAYA( target_val );
  }
  SAYA("\n" );
#endif // OUTPUT_DEBUG


  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(property != nsnull, "null ptr");
  if (! property)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(target != nsnull, "null ptr");
  if (! target)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(hasAssertion != nsnull, "null ptr");
  if (! hasAssertion)
    return NS_ERROR_NULL_POINTER;

  // we only have positive assertions in the file system data source.
  *hasAssertion = PR_FALSE;

  if (! tv) {
    return NS_OK;
  }

  if ( source == kNC_Playlist )
  {
    if (property == kRDF_type)
    {
      nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
      if (resource.get() == kRDF_type)
      {
        *hasAssertion = PR_FALSE;
      }
    }
    if (property == kRDF_InstanceOf)
    {
      // Okay, so, first we'll try to say we're an RDF sequence.  Trees should like those.
      nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
      if (resource.get() == kRDF_Seq)
      {
        *hasAssertion = PR_FALSE;
      }
    }
    else
    {
      SAYA("Has Assertion - ERROR: Unknown property" );
    }
  }

  return NS_OK;
}



NS_IMETHODIMP 
sbPlaylistsource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  SAYA("HasArcIn\n" );
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
sbPlaylistsource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  SAYA("\nHasArcOut\n" );
#if OUTPUT_DEBUG
  const char *source_val = nsnull;
  aSource->GetValueConst( &source_val );
  SAYA("source: " );
  SAYA( source_val );
  const char *property_val = nsnull;
  aArc->GetValueConst( &property_val );
  SAYA("\narc: " );
  SAYA( property_val );
  SAYA("\n" );
#endif // OUTPUT_DEBUG
  *result = PR_FALSE;
  /*
  if (aSource == kNC_Playlist)
  {
  *result = (aArc == kNC_child || aArc == kNC_pulse);
  }
  else if (isFileURI(aSource))
  {
  if (aArc == kNC_pulse)
  {
  *result = PR_TRUE;
  }
  else if (isDirURI(aSource))
  {
  #ifdef  XP_WIN
  *result = isValidFolder(aSource);
  #else
  *result = PR_TRUE;
  #endif
  }
  else if (aArc == kNC_pulse || aArc == kNC_Name || aArc == kNC_Icon ||
  aArc == kNC_URL || aArc == kNC_Length || aArc == kWEB_LastMod ||
  aArc == kNC_FileSystemObject || aArc == kRDF_InstanceOf ||
  aArc == kRDF_type)
  {
  *result = PR_TRUE;
  }
  }
  */
  return NS_OK;
}



NS_IMETHODIMP
sbPlaylistsource::ArcLabelsIn(nsIRDFNode *node,
                      nsISimpleEnumerator ** labels /* out */)
{
  SAYA("ArcLabelsIn\n" );
  //  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
sbPlaylistsource::ArcLabelsOut(nsIRDFResource *source,
                       nsISimpleEnumerator **labels /* out */)
{
  SAYA("ArcLabelsOut\n" );
  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(labels != nsnull, "null ptr");
  if (! labels)
    return NS_ERROR_NULL_POINTER;

  /*
  nsresult rv;

  if (source == kNC_Playlist)
  {
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) return rv;

  array->AppendElement(kNC_child);
  array->AppendElement(kNC_pulse);

  nsISimpleEnumerator* result = new nsArrayEnumerator(array);
  if (! result)
  return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *labels = result;
  return NS_OK;
  }
  else if (isFileURI(source))
  {
  nsCOMPtr<nsISupportsArray> array;
  rv = NS_NewISupportsArray(getter_AddRefs(array));
  if (NS_FAILED(rv)) return rv;

  if (isDirURI(source))
  {
  #ifdef  XP_WIN
  if (isValidFolder(source) == PR_TRUE)
  {
  array->AppendElement(kNC_child);
  }
  #else
  array->AppendElement(kNC_child);
  #endif
  array->AppendElement(kNC_pulse);
  }

  nsISimpleEnumerator* result = new nsArrayEnumerator(array);
  if (! result)
  return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);
  *labels = result;
  return NS_OK;
  }
  */
  return NS_NewEmptyEnumerator(labels);
}



NS_IMETHODIMP
sbPlaylistsource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  SAYA("GetAllResources\n" );
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
sbPlaylistsource::AddObserver(nsIRDFObserver *n)
{
  SAYA("AddObserver\n" );
  NS_PRECONDITION(n != nsnull, "null ptr");
  if (! n)
    return NS_ERROR_NULL_POINTER;

  PRBool found = PR_FALSE;
  for ( observers_t::iterator oi = g_Observers.begin(); oi != g_Observers.end(); oi++ )
  {
    // Find the pointer?
    if ( (*oi).m_Observer == n )
    {
      // Cool, we already knew about this guy?
      found = PR_TRUE;
      // But doublecheck just in case
      if ( (*oi).m_Ref != m_IncomingObserver )
      {
        MessageBoxW( nsnull, (*oi).m_Ref.get(), m_IncomingObserver.get(), MB_OK );
      }
      (*oi).m_Ref = m_IncomingObserver;
      (*oi).m_Ptr = m_IncomingObserverPtr;
    }
  }

  if ( !found )
  {
    // If you didn't call "IncomingObserver()" first, you get ALL updates.
    sbObserver ob;
    ob.m_Observer = n;
    ob.m_Ref = m_IncomingObserver;
    ob.m_Ptr = m_IncomingObserverPtr;
    m_IncomingObserver = NS_LITERAL_STRING( "" ); // Clear the "incoming" ref.
    m_IncomingObserverPtr = nsnull;
    g_Observers.insert( ob );
  }

  // This is stupid, but it works.  Or at least it used to.

  return NS_OK;
}



NS_IMETHODIMP
sbPlaylistsource::RemoveObserver(nsIRDFObserver *n)
{
  SAYA("RemoveObserver\n" );
  NS_PRECONDITION(n != nsnull, "null ptr");
  if (! n)
    return NS_ERROR_NULL_POINTER;

  sbObserver ob;
  ob.m_Observer = n;

  for ( observers_t::iterator oi = g_Observers.begin(); oi != g_Observers.end(); oi++ )
  {
    // Find the pointer?
    if ( (*oi).m_Observer == n )
    {
      g_Observers.erase( oi );
      break;
    }
  }

  return NS_OK;
}



NS_IMETHODIMP
sbPlaylistsource::GetAllCmds(nsIRDFResource* source,
                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  SAYA("GetAllCmds\n" );
  return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
sbPlaylistsource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                           nsIRDFResource*   aCommand,
                           nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                           PRBool* aResult)
{
  SAYA("IsCommandEnabled\n" );
  return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
sbPlaylistsource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                    nsIRDFResource*   aCommand,
                    nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  SAYA("DoCommand\n" );
  return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
sbPlaylistsource::BeginUpdateBatch()
{
  SAYA("BeginUpdateBatch\n" );
  return NS_OK;
}



NS_IMETHODIMP
sbPlaylistsource::EndUpdateBatch()
{
  SAYA("EndUpdateBatch\n" );
  return NS_OK;
}

void
sbPlaylistsource::LoadRowResults( sbPlaylistsource::sbValueInfo & value )
{
  nsAutoMonitor mon(g_pMonitor);
  m_SharedQuery->ResetQuery();
  PRUnichar *g = nsnull;
  value.m_Info->m_Query->GetDatabaseGUID( &g );
  nsString guid( g );
  m_SharedQuery->SetDatabaseGUID( guid.get() );

  PRUnichar *oq = nsnull;
  value.m_Info->m_Query->GetQuery( 0, &oq );
  nsString q( oq );

  // If we already have a where, don't add another one
  nsString aw_str;
  if ( q.Find( "where", PR_TRUE ) == -1 )
  {
    aw_str = NS_LITERAL_STRING( " where " );
  }
  else
  {
    aw_str = NS_LITERAL_STRING( " and " );
  }

  nsString tablerow_str = library_str + dot_str + row_str;
  nsString find_str = spc_str + tablerow_str + spc_str;
  if ( q.Find( find_str ) != -1 )
  {
    q.ReplaceSubstring( find_str, spc_str + NS_LITERAL_STRING("*,") + tablerow_str + spc_str );
    q += aw_str + tablerow_str + spc_str + ge_str + spc_str;
  }
  else
  {
    q.ReplaceSubstring( NS_LITERAL_STRING(" id "), NS_LITERAL_STRING(" *,id ") );
    q += aw_str + NS_LITERAL_STRING( "id >= " );
  }
  q += value.m_Id;

  const PRInt32 LOOKAHEAD_SIZE = 30;
  q += limit_str;
  q.AppendInt( LOOKAHEAD_SIZE );

  m_SharedQuery->AddQuery( q.get() );

  PRInt32 exe;
  mon.Exit();
  m_SharedQuery->Execute( &exe );
  mon.Enter();

  // Stash the result object
  nsCOMPtr< sbIDatabaseResult > result;
  m_SharedQuery->GetResultObjectOrphan( getter_AddRefs( result ) );

  // Walk through the ResList to put the lookahead values into the rows
  PRInt32 i,rows = 0;
  result->GetRowCount( &rows );
  PRInt32 end = min( value.m_ResMapIndex + rows, value.m_Info->m_ResList.size() );
  for ( i = value.m_ResMapIndex; i < end; i++ )
  {
    sbPlaylistsource::sbValueInfo &val = g_ValueMap[ value.m_Info->m_ResList[ i ] ];
    val.m_Resultset = result;
    val.m_ResultsRow = i - value.m_ResMapIndex;
  }

  // Free the strings we got
  PR_Free( g );
  PR_Free( oq );
}


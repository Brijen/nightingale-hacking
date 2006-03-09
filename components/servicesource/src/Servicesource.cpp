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
* \file  Servicesource.cpp
* \brief Songbird Servicesource Component Implementation.
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

#include "nsITimer.h"

#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsEnumeratorUtils.h"

/*
#include "nsIRDFFileSystem.h"
#include "nsRDFBuiltInServicesources.h"
*/

#include "nsCRT.h"
#include "nsRDFCID.h"
#include "nsLiteralString.h"
#include "nsStringAPI.h"

#include "Servicesource.h"

#include "IPlaylist.h"

#define OUTPUT_DEBUG 0

#if OUTPUT_DEBUG
#define SAYA( s ) ::OutputDebugStringA( s )
#define SAY( s ) ::OutputDebugString( s )
#else
#define SAYA( s ) // ::OutputDebugStringA( s )
#define SAY( s ) // ::OutputDebugString( s )
#endif

static  CServicesource  *gServicesource = nsnull;
static  nsIRDFService   *gRDFService = nsnull;

// A callback from the database for when things change and we should repaint.
class MyQueryCallback : public sbIDatabaseSimpleQueryCallback
{
  NS_DECL_ISUPPORTS
    NS_DECL_SBIDATABASESIMPLEQUERYCALLBACK

    MyQueryCallback()
  {
    m_Timer = do_CreateInstance(NS_TIMER_CONTRACTID);
  }

  static void MyTimerCallbackFunc(nsITimer *aTimer, void *aClosure)
  {
    if ( gServicesource )
    {
      gServicesource->Update();
    }
  }
  nsCOMPtr< nsITimer >          m_Timer;
};
NS_IMPL_ISUPPORTS1(MyQueryCallback, sbIDatabaseSimpleQueryCallback)
/* void OnQueryEnd (in sbIDatabaseResult dbResultObject, in wstring dbGUID, in wstring strQuery); */
NS_IMETHODIMP MyQueryCallback::OnQueryEnd(sbIDatabaseResult *dbResultObject, const PRUnichar *dbGUID, const PRUnichar *strQuery)
{
  if ( m_Timer.get() )
  {
    m_Timer->Cancel();
  }

  m_Timer->InitWithFuncCallback( &MyTimerCallbackFunc, gServicesource, 0, 0 );

  return NS_OK;
}

// Lame hardcode to find the playlist folder
const int nPlaylistsInsert = 1;

static nsString gPlaylistUrl( NS_LITERAL_STRING("chrome://Songbird/content/xul/main_pane.xul?") );

static nsString     gParentLabels[ NUM_PARENTS ] =
{
  nsString( NS_LITERAL_STRING("&servicesource.welcome") ),
  nsString( NS_LITERAL_STRING("&servicesource.library") ),
  nsString( NS_LITERAL_STRING("&servicesource.bookmarks") ),
  nsString( NS_LITERAL_STRING("&servicesource.searches") ),
  nsString( NS_LITERAL_STRING("&servicesource.music_stores") ),
  nsString( NS_LITERAL_STRING("&servicesource.podcasts") ),
  nsString( NS_LITERAL_STRING("&servicesource.radio") ),
  nsString( NS_LITERAL_STRING("Network Services") ),
  nsString( NS_LITERAL_STRING("Network Devices") ),
};

static nsString     gParentIcons[ NUM_PARENTS ] =
{
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/logo_16.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_lib_16x16.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/icon_folder.png") ),
};

static nsString     gParentUrls[ NUM_PARENTS ] =
{
  nsString( NS_LITERAL_STRING("http://songbirdnest.com/player/welcome/") ),
  nsString( NS_LITERAL_STRING("chrome://Songbird/content/xul/main_pane.xul?library") ),
  nsString( ),
  nsString( ),
  nsString( ),
  nsString( ),
  nsString( ),
  nsString( ),
  nsString( ),
};

static nsString     gParentOpen[ NUM_PARENTS ] =
{
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("true") ), // true
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
  nsString( NS_LITERAL_STRING("false") ),
};

static nsString     gChildLabels[ NUM_PARENTS ][ MAX_CHILDREN ] =
{
  // Group 1
  {
    nsString( ),
  },
  // Group 2
  {
    nsString( ),
  },
  // Group 7
  {
    nsString( NS_LITERAL_STRING("Pitchfork") ),
    nsString( NS_LITERAL_STRING("Podbop") ),
    nsString( NS_LITERAL_STRING("Swedelife") ),
    nsString( NS_LITERAL_STRING("La Blogoth�que") ),
    nsString( NS_LITERAL_STRING("Medicine") ),
    nsString( NS_LITERAL_STRING("OpenBSD") ),
    nsString( NS_LITERAL_STRING("Songbirdnest") ),
    nsString( ),
  },
  // Group 11
  {
    nsString( NS_LITERAL_STRING("Google") ),
    nsString( NS_LITERAL_STRING("Yahoo!") ),
    nsString( NS_LITERAL_STRING("Creative Commons") ),
    nsString( ),
  },
  // Group 8
  {
    nsString( NS_LITERAL_STRING("Connect") ),
    nsString( NS_LITERAL_STRING("Amazon") ),
    nsString( NS_LITERAL_STRING("Audible") ),
    nsString( NS_LITERAL_STRING("Beatport") ),
    nsString( NS_LITERAL_STRING("MP3Tunes") ),
    nsString( NS_LITERAL_STRING("eMusic") ),
    nsString( NS_LITERAL_STRING("CD Baby") ),
    nsString( NS_LITERAL_STRING("Selectric Records") ),
    nsString( NS_LITERAL_STRING("Arts and Crafts Records") ),
    nsString( ),
  },
  // Group 9
  {
    nsString( NS_LITERAL_STRING("Odeo Featured") ),
    nsString( NS_LITERAL_STRING("Podemus") ),
    nsString( ),
  },
  // Group 10
  {
    nsString( NS_LITERAL_STRING("SHOUTcast") ),
    nsString( NS_LITERAL_STRING("Radiotime") ),
    nsString( NS_LITERAL_STRING("MVY Radio") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("Ninjam") ),
    nsString( NS_LITERAL_STRING("last.fm") ),
    nsString( NS_LITERAL_STRING("Memotrax") ),
    nsString( NS_LITERAL_STRING("AllMusic") ),
    nsString( NS_LITERAL_STRING("MP3Tunes Locker") ),
    nsString( NS_LITERAL_STRING("Streampad") ),
    nsString( NS_LITERAL_STRING("Tables Turned") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("SlimDevices") ),
    nsString( NS_LITERAL_STRING("Sonos") ),
    nsString( ),
  },
};

static nsString     gChildUrls[ NUM_PARENTS ][ MAX_CHILDREN ] =
{
  // Group 1
  {
    nsString( ),
  },
  // Group 2
  {
    nsString( ),
  },
  // Group 7
  {
    nsString( NS_LITERAL_STRING("http://www.pitchforkmedia.com/mp3/")),
    nsString( NS_LITERAL_STRING("http://podbop.org/")),
    nsString( NS_LITERAL_STRING("http://www.swedelife.com/")),
    nsString( NS_LITERAL_STRING("http://www.blogotheque.net/mp3/")),
    nsString( NS_LITERAL_STRING("http://takeyourmedicinemp3.blogspot.com/")),
    nsString( NS_LITERAL_STRING("http://openbsd.mirrors.tds.net/pub/OpenBSD/songs/")),
    nsString( NS_LITERAL_STRING("http://songbirdnest.com/")),
    nsString( ),
  },
  // Group 11
  {
    nsString( NS_LITERAL_STRING("http://www.google.com/musicsearch") ),
    nsString( NS_LITERAL_STRING("http://audio.search.yahoo.com/") ),
    nsString( NS_LITERAL_STRING("http://search.creativecommons.org/") ),
    nsString( ),
  },
  // Group 8
  {
    nsString( NS_LITERAL_STRING("http://musicstore.connect.com/promos/dc/take_tour.html") ),
    nsString( NS_LITERAL_STRING("http://www.faser.net/mab/chrome/content/mab.xul") ),
    nsString( NS_LITERAL_STRING("http://audible.com/") ),
    nsString( NS_LITERAL_STRING("http://beatport.com/") ),
    nsString( NS_LITERAL_STRING("http://www.mp3tunes.com/store.php") ),
    nsString( NS_LITERAL_STRING("http://emusic.com/") ),
    nsString( NS_LITERAL_STRING("http://cdbaby.com/") ),
    nsString( NS_LITERAL_STRING("http://www.selectricrecords.com/") ),
    nsString( NS_LITERAL_STRING("http://www.galleryac.com/") ),
    nsString( ),
  },
  // Group 9
  {
    nsString( NS_LITERAL_STRING("http://odeo.com/listen/featured/") ),
    nsString( NS_LITERAL_STRING("http://podemus.fr/") ),
    nsString( ),
  },
  // Group 10
  {
    nsString( NS_LITERAL_STRING("http://www.shoutcast.com/") ),
    nsString( NS_LITERAL_STRING("http://radiotime.com/") ),
    nsString( NS_LITERAL_STRING("http://www.mvyradio.com/") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("http://ninjam.com/samples.php") ),
    nsString( NS_LITERAL_STRING("http://last.fm/group/Songbird") ),
    nsString( NS_LITERAL_STRING("http://memotrax.com/") ),
    nsString( NS_LITERAL_STRING("http://www.allmusic.com/") ),
    nsString( NS_LITERAL_STRING("http://www.mp3tunes.com/locker/") ),
    nsString( NS_LITERAL_STRING("http://www.streampad.com/") ),
    nsString( NS_LITERAL_STRING("http://tablesturned.com/") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("http://www.slimdevices.com/") ),
    nsString( NS_LITERAL_STRING("http://www.sonos.com/") ),
    nsString( ),
  },
};

static nsString     gChildIcons[ NUM_PARENTS ][ MAX_CHILDREN ] =
{
  // Group 1
  {
    nsString( ),
  },
  // Group 2
  {
    nsString( ),
  },
  // Group 7
  {
    nsString( NS_LITERAL_STRING("http://www.pitchforkmedia.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("http://podbop.org/favicon.ico")),
    nsString( NS_LITERAL_STRING("http://www.swedelife.com/favicon.ico")),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/default/logo_16.png") ),
    nsString( ),
  },
  // Group 11
  {
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/google.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/yahoo.ico") ),
    nsString( NS_LITERAL_STRING("http://creativecommons.org/favicon.ico") ),
    nsString( ),
  },
  // Group 8
  {
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/connect.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/amazon.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("https://www.beatport.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("http://www.mp3tunes.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/emusic.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/cdbaby.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( ),
  },
  // Group 9
  {
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/odeo.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( ),
  },
  // Group 10
  {
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/shoutcast.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/radiotime.ico") ),
    nsString( NS_LITERAL_STRING("http://www.mvyradio.com/favicon.ico") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/ninjam_gui_win.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("http://www.allmusic.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("http://mp3tunes.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("chrome://Songbird/skin/serviceicons/pogues.ico") ),
    nsString( NS_LITERAL_STRING("http://tablesturned.com/favicon.ico") ),
    nsString( ),
  },
  {
    nsString( NS_LITERAL_STRING("http://slimdevices.com/favicon.ico") ),
    nsString( NS_LITERAL_STRING("http://www.sonos.com/favicon.ico") ),
    nsString( ),
  },
};

// CLASSES ====================================================================
NS_IMPL_ISUPPORTS2(CServicesource, sbIServicesource, nsIRDFDataSource)
//-----------------------------------------------------------------------------
CServicesource::CServicesource()
{
  Init();
} //ctor

//-----------------------------------------------------------------------------
/*virtual*/ CServicesource::~CServicesource()
{
  DeInit();

} //dtor

void CServicesource::Update(void)
{
  // Inform the observers that everything changed.
  for ( observers_t::iterator o = m_Observers.begin(); o != m_Observers.end(); o++ )
  {
    (*o)->OnBeginUpdateBatch( this );
    (*o)->OnEndUpdateBatch( this );
  }
}

void CServicesource::Init(void)
{
  {
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

    // Get the RDF service
    if ( nsnull == gRDFService )
    {
      rv = CallGetService("@mozilla.org/rdf/rdf-service;1", &gRDFService);
    }
    if ( NS_SUCCEEDED(rv) )
    {
      gRDFService->GetResource(NS_LITERAL_CSTRING("NC:Servicesource"),
        &kNC_Servicesource);
      gRDFService->GetResource(NS_LITERAL_CSTRING("NC:ServicesourceFlat"),
        &kNC_ServicesourceFlat);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Label"),
        &kNC_Label);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Icon"),
        &kNC_Icon);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "URL"),
        &kNC_URL);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Properties"),
        &kNC_Properties);
      gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI  "Open"),
        &kNC_Open);

      int i,j;
      for ( i = 0; i < NUM_PARENTS; i++ )
      {
        gRDFService->GetAnonymousResource( &(kNC_Parent[ i ]) );
      }
      for ( i = 0; i < NUM_PARENTS; i++ )
      for ( j = 0; j < MAX_CHILDREN; j++ )
      {

        gRDFService->GetAnonymousResource( &(kNC_Child[ i ][ j ]) );
      }

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
      gRDFService->GetResource(NS_LITERAL_CSTRING(RDF_NAMESPACE_URI "Seq"),
        &kRDF_Seq);

      gRDFService->GetLiteral(NS_LITERAL_STRING("true").get(),       
        &kLiteralTrue);
      gRDFService->GetLiteral(NS_LITERAL_STRING("false").get(),      
        &kLiteralFalse);
    }
    else
    {
      SAYA( "Init - No gRDFService, can't get resources.\n" );
    }

    // Setup the list of playlists as a persistent query
    m_PlaylistsQuery = do_CreateInstance( "@songbird.org/Songbird/DatabaseQuery;1" );
    m_PlaylistsQuery->SetAsyncQuery( PR_TRUE );
    m_PlaylistsQuery->SetDatabaseGUID( NS_LITERAL_STRING("songbird").get() );

    MyQueryCallback *callback = new MyQueryCallback;
    callback->AddRef();
    m_PlaylistsQuery->AddSimpleQueryCallback( callback );
    m_PlaylistsQuery->SetPersistentQuery( true );

    nsCOMPtr< sbIPlaylistManager > pPlaylistManager = do_CreateInstance( "@songbird.org/Songbird/PlaylistManager;1" );
    if(pPlaylistManager.get())
      pPlaylistManager->GetAllPlaylistList( m_PlaylistsQuery );

    gServicesource = this;
  }
}

void CServicesource::DeInit (void)
{
#ifdef DEBUG_REFS
  --gInstanceCount;
  fprintf(stdout, "%d - RDF: CServicesource\n", gInstanceCount);
#endif
  {
    if ( nsnull != gRDFService )
    {

      NS_RELEASE(kNC_Servicesource);
      NS_RELEASE(kNC_ServicesourceFlat);
      NS_RELEASE(kNC_Label);
      NS_RELEASE(kNC_Icon);
      NS_RELEASE(kNC_URL);
      NS_RELEASE(kNC_Properties);
      NS_RELEASE(kNC_Open);
      int i,j;
      for ( i = 0; i < NUM_PARENTS; i++ )
      {
        NS_RELEASE(kNC_Parent[ i ]);
      }
      for ( i = 0; i < NUM_PARENTS; i++ )
      for ( j = 0; j < MAX_CHILDREN; j++ )
      {
        NS_RELEASE(kNC_Child[ i ][ j ]);
      }
      NS_RELEASE(kRDF_InstanceOf);
      NS_RELEASE(kRDF_type);
      NS_RELEASE(gRDFService);
    }
    gServicesource = nsnull;
  }
}

NS_IMETHODIMP
CServicesource::GetURI(char **uri)
{
  SAYA( "GetURI\n" );
  NS_PRECONDITION(uri != nsnull, "null ptr");
  if (! uri)
    return NS_ERROR_NULL_POINTER;

  if ((*uri = nsCRT::strdup("rdf:Servicesource")) == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}



NS_IMETHODIMP
CServicesource::GetSource(nsIRDFResource* property,
                    nsIRDFNode* target,
                    PRBool tv,
                    nsIRDFResource** source /* out */)
{
  SAYA( "GetSource\n" );
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
CServicesource::GetSources(nsIRDFResource *property,
                     nsIRDFNode *target,
                     PRBool tv,
                     nsISimpleEnumerator **sources /* out */)
{
  SAYA( "GetSources\n" );
  //  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
CServicesource::GetTarget(nsIRDFResource *source,
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

  SAYA( "\nGetTarget\n" );

#if OUTPUT_DEBUG
  const char *source_val = NULL;
  source->GetValueConst( &source_val );
  SAYA( "source: " );
  SAYA( source_val );
  const char *property_val = NULL;
  property->GetValueConst( &property_val );
  SAYA( "\nproperty: " );
  SAYA( property_val );
  SAYA( "\n" );
#endif // OUTPUT_DEBUG

  *target = nsnull;

  nsresult        rv = NS_RDF_NO_VALUE;

  // we only have positive assertions in the file system data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsString outstring;

  if ( ! outstring.Length() )
  {
    int i,j;
    for ( i = 0; i < NUM_PARENTS; i++ )
    {
      if ( source == kNC_Parent[ i ])
      {
        if (property == kNC_Label)
        {
          outstring = gParentLabels[ i ];
        }
        else if (property == kNC_Icon)
        {
          outstring = gParentIcons[ i ];
        }
        else if (property == kNC_URL)
        {
          outstring = gParentUrls[ i ];
        }
        else if (property == kNC_Properties)
        {
          outstring = NS_LITERAL_STRING( "" );
        }
        else if (property == kNC_Open)
        {
          outstring = gParentOpen[ i ];
        }
      }
    }
    for ( i = 0; i < NUM_PARENTS; i++ )
    for ( j = 0; j < MAX_CHILDREN; j++ )
    {
      if ( source == kNC_Child[ i ][ j ] )
      {
        if (property == kNC_Label)
        {
          outstring = gChildLabels[ i ][ j ];
        }
        else if (property == kNC_Icon)
        {
          outstring = gChildIcons[ i ][ j ];
        }
        else if (property == kNC_URL)
        {
          outstring = gChildUrls[ i ][ j ];
        }
        else if (property == kNC_Properties)
        {
          outstring = NS_LITERAL_STRING( "" );
        }
        else if (property == kNC_Open)
        {
          outstring = NS_LITERAL_STRING( "false" );
        }
      }
    }
  }


  // If it's not a hardcode source, check the playlists
  if ( ! outstring.Length() )
  {
    resmap_t::iterator pl = m_PlaylistMap.find( source );
    if ( pl != m_PlaylistMap.end() )
    {
      sbIDatabaseResult *resultset;
      m_PlaylistsQuery->GetResultObject( &resultset );

      if (property == kNC_Label)
      {
        PRUnichar* data;
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("readable_name").get(), &data);
        outstring = data;
        PR_Free( data );
      }
      else if (property == kNC_Icon)
      {
        // These icons come from the skin.
//        outstring = NS_LITERAL_STRING( "chrome://Songbird/skin/default/icon_lib_16x16.png" );
      }
      else if (property == kNC_URL)
      {
        PRUnichar* data;
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("name").get(), &data);
        outstring = gPlaylistUrl + nsString( data );
        PR_Free( data );
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("service_uuid").get(), &data);
        outstring += NS_LITERAL_STRING(",") + nsString( data );
        PR_Free( data );
      }
      else if (property == kNC_Properties)
      {
        // Javascript code is assuming this ordering!
        outstring = NS_LITERAL_STRING( "playlist" );
        PRUnichar* data;
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("name").get(), &data);
        outstring += NS_LITERAL_STRING(" ") + nsString( data );
        PR_Free( data );
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("service_uuid").get(), &data);
        outstring += NS_LITERAL_STRING(" ") + nsString( data );
        PR_Free( data );
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("type").get(), &data);
        outstring += NS_LITERAL_STRING(" ") + nsString( data );
        PR_Free( data );
        resultset->GetRowCellByColumn( (*pl).second, NS_LITERAL_STRING("base_type").get(), &data);
        outstring += NS_LITERAL_STRING(" ") + nsString( data );
        PR_Free( data );
      }
      else if (property == kNC_Open)
      {
        outstring = NS_LITERAL_STRING("false");
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
    if (rv == NS_RDF_NO_VALUE)  return(rv);

    return literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) target);
  }

  return(NS_RDF_NO_VALUE);
}



NS_IMETHODIMP
CServicesource::GetTargets(nsIRDFResource *source,
                     nsIRDFResource *property,
                     PRBool tv,
                     nsISimpleEnumerator **targets /* out */)
{
  SAYA( "\nGetTargets\n" );

/*
  PRBool exec = false;
  m_PlaylistsQuery->IsExecuting( &exec );
  if ( ! exec )
  {
    PRInt32 error;
    m_PlaylistsQuery->GetLastError( &error );
    if ( error )
    {
      nsCOMPtr< sbIPlaylistManager > pPlaylistManager = do_CreateInstance( "@songbird.org/Songbird/PlaylistManager;1" );
      if(pPlaylistManager.get())
        pPlaylistManager->GetAllPlaylistList( m_PlaylistsQuery );
      m_PlaylistsQuery->GetLastError( &error );
      if ( error )
      {
//        __asm int 3;
      }
    }
  }
*/

#if OUTPUT_DEBUG
  const char *source_val = NULL;
  source->GetValueConst( &source_val );
  SAYA( "source: " );
  SAYA( source_val );
  const char *property_val = NULL;
  property->GetValueConst( &property_val );
  SAYA( "\nproperty: " );
  SAYA( property_val );
  SAYA( "\n" );
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

  // we only have positive assertions in the data source.
  if (! tv)
    return NS_RDF_NO_VALUE;

  nsresult rv;

  if (source == kNC_Servicesource)
  {
    if (property == kNC_child)
    {
      SAYA( "GetTargets - kNC_child\n" );
      nsresult rv = NS_OK;
      *targets = NULL;

      // Make the array to hold our response
      nsCOMPtr<nsISupportsArray> nextItemArray;
      rv = NS_NewISupportsArray(getter_AddRefs(nextItemArray));
      if (NS_FAILED(rv)) return rv;

      // Append items to the array
      for ( int i = 0; i < NUM_PARENTS; i++ )
      {
        nextItemArray->AppendElement( kNC_Parent[ i ] );
        // Magic stuff to list the playlists
        if ( i == nPlaylistsInsert )
        {
          sbIDatabaseResult *resultset;
          m_PlaylistsQuery->GetResultObject( &resultset );
          PRInt32 num_rows;
          PRInt32 num_cols;
          resultset->GetRowCount( &num_rows );
          resultset->GetColumnCount( &num_cols );

          PRInt32 j;
          for( j = 0; j < num_rows; j++ )
          {
            // Create a new resource for this item
            nsIRDFResource *next_resource;
            gRDFService->GetAnonymousResource( &next_resource );
            nextItemArray->AppendElement( next_resource );
            m_PlaylistMap[ next_resource ] = j;
          }

          resultset->Release();
        }
      }

      // Stuff the array into the enumerator
      nsISimpleEnumerator* result = new nsArrayEnumerator(nextItemArray);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

      // And give it to whomever is calling us
      NS_ADDREF(result);
      *targets = result;

      return NS_OK;
    }
  }
  else if (source == kNC_ServicesourceFlat)
  {
    if (property == kNC_child)
    {
      SAYA( "GetTargets - kNC_child\n" );
      nsresult rv = NS_OK;
      *targets = NULL;

      // Make the array to hold our response
      nsCOMPtr<nsISupportsArray> nextItemArray;
      rv = NS_NewISupportsArray(getter_AddRefs(nextItemArray));
      if (NS_FAILED(rv)) return rv;

      // Append items to the array
      for ( int i = 0; i < NUM_PARENTS; i++ )
      {
        // Only add the parent if it has no children
        if ( ! gChildLabels[ i ][ 0 ].Length() )
        {
          nextItemArray->AppendElement( kNC_Parent[ i ] );
        }
        else
        {
          // Otherwise add the children
          for ( int j = 0; j < MAX_CHILDREN; j++ )
          {
            // If there is a child, append it.
            if ( gChildLabels[ i ][ j ].Length() )
            {
              nextItemArray->AppendElement( kNC_Child[ i ][ j ] );
            }
          }
        }
      }
      // Stuff the array into the enumerator
      nsISimpleEnumerator* result = new nsArrayEnumerator(nextItemArray);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

      // And give it to whomever is calling us
      NS_ADDREF(result);
      *targets = result;

      return NS_OK;
    }
  }
  else 
  {
    if ( property == kNC_child )
    {
      int i,j;
      for ( i = 0; i < NUM_PARENTS; i++ )
      {
        if ( source == kNC_Parent[ i ])
        {
          nsresult rv = NS_OK;
          *targets = NULL;

          // Make the array to hold our response
          nsCOMPtr<nsISupportsArray> nextItemArray;
          rv = NS_NewISupportsArray(getter_AddRefs(nextItemArray));
          if (NS_FAILED(rv)) return rv;

          {
            // Append items to the array
            for ( j = 0; j < MAX_CHILDREN; j++ )
            {
              // If there is a label for this element, append it.
              if ( gChildLabels[ i ][ j ].Length() )
              {
#if OUTPUT_DEBUG
                const char *property_val = NULL;
                kNC_Child[ i ][ j ]->GetValueConst( &property_val );
                SAYA( "target: " );
                SAYA( property_val );
                SAYA( " : " );

                SAY( PromiseFlatString( gChildLabels[ i ][ j ] ).get() );
                SAYA( "\n" );
#endif
                nextItemArray->AppendElement( kNC_Child[ i ][ j ] );
              }
            }
          }

          // Stuff the array into the enumerator
          nsISimpleEnumerator* result = new nsArrayEnumerator(nextItemArray);
          if (! result)
            return NS_ERROR_OUT_OF_MEMORY;

          // And give it to whomever is calling us
          NS_ADDREF(result);
          *targets = result;

          return NS_OK;
        }
      }
    }
  }
  return NS_NewEmptyEnumerator(targets);
}



NS_IMETHODIMP
CServicesource::Assert(nsIRDFResource *source,
                 nsIRDFResource *property,
                 nsIRDFNode *target,
                 PRBool tv)
{
  SAYA( "Assert\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
CServicesource::Unassert(nsIRDFResource *source,
                   nsIRDFResource *property,
                   nsIRDFNode *target)
{
  SAYA( "Unassert\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
CServicesource::Change(nsIRDFResource* aSource,
                 nsIRDFResource* aProperty,
                 nsIRDFNode* aOldTarget,
                 nsIRDFNode* aNewTarget)
{
  SAYA( "Change\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
CServicesource::Move(nsIRDFResource* aOldSource,
               nsIRDFResource* aNewSource,
               nsIRDFResource* aProperty,
               nsIRDFNode* aTarget)
{
  SAYA( "Move\n" );
  return NS_RDF_ASSERTION_REJECTED;
}



NS_IMETHODIMP
CServicesource::HasAssertion(nsIRDFResource *source,
                       nsIRDFResource *property,
                       nsIRDFNode *target,
                       PRBool tv,
                       PRBool *hasAssertion /* out */)
{
  SAYA( "\nHasAssertion\n" );

#if OUTPUT_DEBUG
  const char *source_val = NULL;
  source->GetValueConst( &source_val );
  SAYA( "source: " );
  SAYA( source_val );
  const char *property_val = NULL;
  property->GetValueConst( &property_val );
  SAYA( "\nproperty: " );
  SAYA( property_val );
  const char *target_val = NULL;
  nsCOMPtr<nsIRDFResource> resource( do_QueryInterface(target) );
  if ( resource.get() )
  {
    resource->GetValueConst( &target_val );
    SAYA( "\ntarget: " );
    SAYA( target_val );
  }
  SAYA( "\n" );
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

//  if ( source == kNC_Servicesource )
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
      SAYA( "Has Assertion - ERROR: Unknown property" );
    }
  }

  return NS_OK;
}



NS_IMETHODIMP 
CServicesource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  SAYA( "HasArcIn\n" );
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP 
CServicesource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  SAYA( "\nHasArcOut\n" );
#if OUTPUT_DEBUG
  const char *source_val = NULL;
  aSource->GetValueConst( &source_val );
  SAYA( "source: " );
  SAYA( source_val );
  const char *property_val = NULL;
  aArc->GetValueConst( &property_val );
  SAYA( "\narc: " );
  SAYA( property_val );
  SAYA( "\n" );
#endif // OUTPUT_DEBUG
  *result = PR_FALSE;

  if (aSource == kNC_Servicesource)
  {
    *result = ( aArc == kNC_child );
  }
  else
  {
    int i;
    for ( i = 0; i < NUM_PARENTS; i++ )
    {
      if ( aSource == kNC_Parent[ i ] )
      {
        *result = ( aArc == kNC_child );
      }
    }
  }
  return NS_OK;
}



NS_IMETHODIMP
CServicesource::ArcLabelsIn(nsIRDFNode *node,
                      nsISimpleEnumerator ** labels /* out */)
{
  SAYA( "ArcLabelsIn\n" );
  //  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
CServicesource::ArcLabelsOut(nsIRDFResource *source,
                       nsISimpleEnumerator **labels /* out */)
{
  SAYA( "ArcLabelsOut\n" );
  NS_PRECONDITION(source != nsnull, "null ptr");
  if (! source)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(labels != nsnull, "null ptr");
  if (! labels)
    return NS_ERROR_NULL_POINTER;

  /*
  nsresult rv;

  if (source == kNC_Servicesource)
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
CServicesource::GetAllResources(nsISimpleEnumerator** aCursor)
{
  SAYA( "GetAllResources\n" );
  NS_NOTYETIMPLEMENTED("sorry!");
  return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
CServicesource::AddObserver(nsIRDFObserver *n)
{
  SAYA( "AddObserver\n" );
  NS_PRECONDITION(n != nsnull, "null ptr");
  if (! n)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRDFObserver> ob = n;
  m_Observers.insert( ob );

  return NS_OK;
}



NS_IMETHODIMP
CServicesource::RemoveObserver(nsIRDFObserver *n)
{
  SAYA( "RemoveObserver\n" );
  NS_PRECONDITION(n != nsnull, "null ptr");
  if (! n)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRDFObserver> ob = n;

  observers_t::iterator o = m_Observers.find( ob );
  if ( o != m_Observers.end() )
  {
    m_Observers.erase( o );
  }

  return NS_OK;
}


NS_IMETHODIMP
CServicesource::GetAllCmds(nsIRDFResource* source,
                     nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
  SAYA( "GetAllCmds\n" );
  return(NS_NewEmptyEnumerator(commands));
}



NS_IMETHODIMP
CServicesource::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                           nsIRDFResource*   aCommand,
                           nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                           PRBool* aResult)
{
  SAYA( "IsCommandEnabled\n" );
  return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
CServicesource::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                    nsIRDFResource*   aCommand,
                    nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  SAYA( "DoCommand\n" );
  return(NS_ERROR_NOT_IMPLEMENTED);
}



NS_IMETHODIMP
CServicesource::BeginUpdateBatch()
{
  SAYA( "BeginUpdateBatch\n" );
  return NS_OK;
}



NS_IMETHODIMP
CServicesource::EndUpdateBatch()
{
  SAYA( "EndUpdateBatch\n" );
  return NS_OK;
}




;--------------------------------
;Variables

!macro MUI_LANGDLL_COUNTRY_VARIABLES

  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_ROOT & MUI_LANGDLL_COUNTRY_REGISTRY_KEY & MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME
    !ifndef MUI_LANGDLL_COUNTRY_REGISTRY_VARIABLES
      !define MUI_LANGDLL_COUNTRY_REGISTRY_VARIABLES

      ;/GLOBAL because the macros are included in a function
      Var /GLOBAL mui.LangDLL.RegistryCountry

    !endif
  !endif

!macroend


;--------------------------------
;Include langauge files

!macro MUI_COUNTRY COUNTRYNAME COUNTRYCODE

  ;Include a country

  !verbose push
  !verbose ${MUI_VERBOSE}

  !insertmacro MUI_INSERT

  !ifndef MUI_LANGDLL_COUNTRY
	!ifdef COUNTRYCODE
		!define MUI_LANGDLL_COUNTRY "'${COUNTRYNAME}' '${COUNTRYCODE}'"
	!else
		!define MUI_LANGDLL_COUNTRY "'${COUNTRYNAME}' '${COUNTRYNAME}'"
	!endif
  !else
    !ifdef MUI_LANGDLL_COUNTRY_TEMP
      !undef MUI_LANGDLL_COUNTRY_TEMP
    !endif
    !define MUI_LANGDLL_COUNTRY_TEMP "${MUI_LANGDLL_COUNTRY}"
    !undef MUI_LANGDLL_COUNTRY
	!ifdef COUNTRYCODE
		!define MUI_LANGDLL_COUNTRY "'${COUNTRYNAME}' '${COUNTRYCODE}' ${MUI_LANGDLL_COUNTRY_TEMP}"
	!else
		!define MUI_LANGDLL_COUNTRY "'${COUNTRYNAME}' '${COUNTRYNAME}' ${MUI_LANGDLL_COUNTRY_TEMP}"		
	!endif
  !endif

  !verbose pop

!macroend


;--------------------------------
;Language selection

!macro MUI_LANGDLL_COUNTRY_DISPLAY

  !verbose push
  !verbose ${MUI_VERBOSE}

  !insertmacro MUI_LANGDLL_COUNTRY_VARIABLES

  !insertmacro MUI_DEFAULT MUI_LANGDLL_COUNTRY_WINDOWTITLE "$(countrySelectionTitle)"
  !insertmacro MUI_DEFAULT MUI_LANGDLL_COUNTRY_INFO "$(countrySelectionString)"

  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_VARIABLES

    ReadRegStr $mui.LangDLL.RegistryCountry "${MUI_LANGDLL_COUNTRY_REGISTRY_ROOT}" "${MUI_LANGDLL_COUNTRY_REGISTRY_KEY}" "${MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME}"
    
    ${if} $mui.LangDLL.RegistryCountry != ""
      ;Set default langauge to registry language
      StrCpy $COUNTRY $mui.LangDLL.RegistryCountry
    ${endif}

  !endif

  !ifdef NSIS_CONFIG_SILENT_SUPPORT
    ${unless} ${Silent}
  !endif

  !ifndef MUI_LANGDLL_COUNTRY_ALWAYSSHOW
  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_VARIABLES
    ${if} $mui.LangDLL.RegistryCountry == ""
  !endif
  !endif
    
  ;Show langauge selection dialog
    LangDLL::LangDialog "${MUI_LANGDLL_COUNTRY_WINDOWTITLE}" "${MUI_LANGDLL_COUNTRY_INFO}" A ${MUI_LANGDLL_COUNTRY} ""
  
    Pop $COUNTRY
    ${if} $COUNTRY == "cancel"
      Abort
    ${endif}
  
  !ifndef MUI_LANGDLL_COUNTRY_ALWAYSSHOW
  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_VARIABLES
    ${endif}
  !endif
  !endif


  !ifdef NSIS_CONFIG_SILENT_SUPPORT
    ${endif}
  !endif

  !verbose pop

!macroend

!macro MUI_LANGDLL_SAVECOUNTRY

  ;Save language in registry
    IfAbort mui.langdllsavecountry_abort

    !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_ROOT & MUI_LANGDLL_COUNTRY_REGISTRY_KEY & MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME
      WriteRegStr "${MUI_LANGDLL_COUNTRY_REGISTRY_ROOT}" "${MUI_LANGDLL_COUNTRY_REGISTRY_KEY}" "${MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME}" $COUNTRY
    !endif

    mui.langdllsavecountry_abort:
!macroend

!macro MUI_UNGETCOUNTRY

  ;Get country from registry in uninstaller

  !verbose push
  !verbose ${MUI_VERBOSE}

  !insertmacro MUI_LANGDLL_COUNTRY_VARIABLES

  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_ROOT & MUI_LANGDLL_COUNTRY_REGISTRY_KEY & MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME

    ReadRegStr $mui.LangDLL.RegistryCountry "${MUI_LANGDLL_COUNTRY_REGISTRY_ROOT}" "${MUI_LANGDLL_COUNTRY_REGISTRY_KEY}" "${MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME}"
    
    ${if} $mui.LangDLL.RegistryCountry = ""

  !endif

  !insertmacro MUI_LANGDLL_COUNTRY_DISPLAY

  !ifdef MUI_LANGDLL_COUNTRY_REGISTRY_ROOT & MUI_LANGDLL_COUNTRY_REGISTRY_KEY & MUI_LANGDLL_COUNTRY_REGISTRY_VALUENAME

    ${else}
      StrCpy $COUNTRY $mui.LangDLL.RegistryCountry
    ${endif}

  !endif

  !verbose pop

!macroend

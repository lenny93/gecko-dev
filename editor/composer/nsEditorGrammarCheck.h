#ifndef GrammarCheck_h
#define GrammarCheck_h

#include "nsCOMPtr.h"                   // for nsCOMPtr
#include "nsCycleCollectionParticipant.h"
#include "nsIEditorGrammarCheck.h"
#include "nsISupportsImpl.h"
#include "nsString.h"                   // for nsString
#include "nsTArray.h"                   // for nsTArray
#include "nscore.h"    
#include "../../extensions/spellcheck/src/mozInlineSpellWordUtil.h"
#include "nsIArray.h"
#include "nsIMutableArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIRunnable.h"

class nsIEditor;

#define NS_GRAMMARCHECK_CONTRACTID "@mozilla.org/grammarcheck;1"
#define NS_GRAMMARCHECK_CLASSNAME "nsEditorGrammarCheck"
#define NS_GRAMMARCHECK_CID { 0x8ac26150, 0x586f, 0x4b70, \
  { 0x90, 0xdf, 0x07, 0x61, 0x0b, 0x80, 0xb4, 0x5f } }


class nsEditorGrammarCheck final : public nsIEditorGrammarCheck
{
public:
  nsEditorGrammarCheck();

  struct GrammarError
  {
    uint32_t mErrorStart;
    uint32_t mErrorEnd;
    nsTArray<nsString> mSuggestions;
    nsTArray<nsString> mDescriptions;
    nsTArray<bool> mMessageOnly;
  };
  
  static already_AddRefed<nsEditorGrammarCheck> GetSingleton();
  
  static nsEditorGrammarCheck* GetGrammarCheckService()
  {
    if (!gGrammarCheckService) {
      nsCOMPtr<nsIEditorGrammarCheck> serv = do_GetService(NS_GRAMMARCHECK_CONTRACTID);
      NS_ENSURE_TRUE(serv, nullptr);
      NS_ASSERTION(gGrammarCheckService, "Should have static instance pointer now");
    }
    return gGrammarCheckService;
  }
  
  NS_IMETHODIMP Init();

  void SetCurrentEditor(nsIEditor* aEditor);

  NS_IMETHODIMP DoGrammarCheck();

    /**
     * This macro expands into a declaration of the nsISupports interface.
     * Every XPCOM component needs to implement nsISupports, as it acts
     * as the gateway to other interfaces this component implements.  You
     * could manually declare QueryInterface, AddRef, and Release instead
     * of using this macro, but why?
     */
    // nsISupports interface
    NS_DECL_ISUPPORTS

    /**
     * This macro is defined in the nsISample.h file, and is generated
     * automatically by the xpidl compiler.  It expands to
     * declarations of all of the methods required to implement the
     * interface.  xpidl will generate a NS_DECL_[INTERFACENAME] macro
     * for each interface that it processes.
     *
     * The methods of nsISample are discussed individually below, but
     * commented out (because this macro already defines them.)
     */
   NS_DECL_NSIEDITORGRAMMARCHECK


  nsCOMPtr<nsIEditor> mEditor;
  nsCOMPtr<nsIDOMNode> mRootNode;
  bool mCheckEnabled;
  
private:
  ~nsEditorGrammarCheck();

  static nsEditorGrammarCheck* gGrammarCheckService;

  nsCOMPtr<nsIEditorGrammarCheckCallback> gCallback;

  nsTArray<GrammarError> mErrors;
};

#endif
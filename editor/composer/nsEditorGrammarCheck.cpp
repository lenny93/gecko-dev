#include <iostream>                     // for getenv
#include <string.h>                     // for nullptr, strcmp
#include "mozilla/Attributes.h"         // for final
#include "mozilla/Preferences.h"        // for Preferences
#include "mozilla/Services.h"           // for GetXULChromeRegistryService
#include "mozilla/dom/Element.h"        // for Element
#include "mozilla/dom/Selection.h"
#include "mozilla/mozalloc.h"           // for operator delete, etc
#include "mozilla/dom/Selection.h"
#include "mozilla/ModuleUtils.h"
#include "nsIEditor.h"                  // for nsIEditor
#include "nsMemory.h"
#include "nsIClassInfoImpl.h"
#include "nsEditorGrammarCheck.h"
#include "nsString.h"
#include "nsXPCOMCIDInternal.h"
#include "nsIServiceManager.h"
#include "nsIDocumentEncoder.h"
#include "nsIHTMLDocument.h"
#include "nsContentCID.h"
#include "nsGkAtoms.h"

using namespace mozilla;


NS_IMPL_CLASSINFO(nsEditorGrammarCheck, nullptr, 0, NS_GRAMMARCHECK_CID)
NS_IMPL_ISUPPORTS_CI(nsEditorGrammarCheck, nsIEditorGrammarCheck)


nsEditorGrammarCheck * nsEditorGrammarCheck::gGrammarCheckService = nullptr;

nsEditorGrammarCheck::nsEditorGrammarCheck() 
  : gCallback(nullptr),
    mEditor(),
    mCheckEnabled(true)
{
  NS_ASSERTION(!gGrammarCheckService,
         "Attempting to create two instances of the service!");
  gGrammarCheckService = this;

}

nsEditorGrammarCheck::~nsEditorGrammarCheck()
{
  NS_ASSERTION(gGrammarCheckService == this,
               "Deleting a non-singleton instance of the service");
  if (gGrammarCheckService == this)
    gGrammarCheckService = nullptr;

}

already_AddRefed<nsEditorGrammarCheck>
nsEditorGrammarCheck::GetSingleton()
{
  if (gGrammarCheckService) {
    nsRefPtr<nsEditorGrammarCheck> ret = gGrammarCheckService;
    return ret.forget();
  }
  gGrammarCheckService = new nsEditorGrammarCheck();
  nsRefPtr<nsEditorGrammarCheck> ret = gGrammarCheckService;
  if (NS_FAILED(gGrammarCheckService->Init())) {
    /* Null out ret before gGrammarCheckService so the destructor doesn't assert */
    ret = nullptr;
    gGrammarCheckService = nullptr;
    return nullptr;
  }
  return ret.forget();
}

NS_IMETHODIMP nsEditorGrammarCheck::Init()
{
  mCheckEnabled = Preferences::GetBool("grammarCheckEnabled", false);
  Preferences::SetBool("grammarCheckEnabled", mCheckEnabled);

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::RegisterAddon(nsIEditorGrammarCheckCallback* callback)
{
  gCallback = callback;
  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::ErrorsFound(uint32_t* errorsStart, uint32_t* errorsEnd, uint32_t count)
{
  if (!mEditor || !mCheckEnabled || !errorsStart || !errorsEnd)
    return NS_OK;

  nsresult rv;

  mErrors.Clear();

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mEditor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(domDoc, NS_ERROR_NULL_POINTER);


  nsCOMPtr<nsIDocument> mDocument = do_QueryInterface(domDoc);

  // Find the root node for the editor. For contenteditable we'll need something
  // cleverer here.
  nsCOMPtr<nsIDOMElement> rootElem;
  rv = mEditor->GetRootElement(getter_AddRefs(rootElem));
  NS_ENSURE_SUCCESS(rv, rv);

  mRootNode = do_QueryInterface(rootElem);
  NS_ASSERTION(mRootNode, "GetRootElement returned null *and* claimed to suceed!");

  if (!mRootNode)
    return NS_OK;

  nsCOMPtr<nsISelectionController> selcon;
  rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

  nsCOMPtr<nsISelection> grammarCheckSelection;
  selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

  if (!grammarCheckSelection)
    return NS_OK;

  grammarCheckSelection->RemoveAllRanges();
  

  for (uint32_t i = 0; i < count; i++) {
    nsCOMPtr<nsIDOMRange> range;
    domDoc->CreateRange(getter_AddRefs(range));

    nsCOMPtr<nsIDOMNode> aStartNode;
    int32_t aStartOffset = 0;
    nsCOMPtr<nsIDOMNode> aEndNode;
    int32_t aEndOffset = -1;


    nsCOMPtr<nsIDOMNodeList> childNodes;
    mRootNode->GetChildNodes(getter_AddRefs(childNodes));

    uint32_t childCount;
    childNodes->GetLength(&childCount);

    if (childCount < 1)
      return NS_OK;


    childNodes->Item(0, getter_AddRefs(aEndNode));
    aStartNode = aEndNode;

    aEndOffset = errorsEnd[i];
    aStartOffset = errorsStart[i];


    // sometimes we are are requested to check an empty range (possibly an empty
    // document). This will result in assertions later.
    if (aStartNode == aEndNode && aStartOffset == aEndOffset)
      return NS_OK;


    rv = range->SetStart(aStartNode, aStartOffset);
    NS_ENSURE_SUCCESS(rv, rv);

    if (aEndOffset) {
      range->SetEnd(aEndNode, aEndOffset);
    } else {
      range->SetEndAfter(aEndNode);
    }


    grammarCheckSelection->AddRange(range);

    GrammarError ge;
    ge.mErrorStart = errorsStart[i];
    ge.mErrorEnd = errorsEnd[i];

    mErrors.AppendElement(ge);
  }


  selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::AddSuggestionForError(uint32_t aError, const nsAString& aSuggestion, const nsAString& aDescription, bool aMessageOnly)
{
  if (mErrors.Length() <= (int)aError)
    return NS_OK;

  

  mErrors[aError].mSuggestions.AppendElement(nsString(aSuggestion));
  mErrors[aError].mDescriptions.AppendElement(nsString(aDescription));
  mErrors[aError].mMessageOnly.AppendElement(aMessageOnly);
  
  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::IsSuggestionMessageOnly(uint32_t aOffset, uint32_t aSuggestion, bool* aRetVal)
{
  for (uint32_t i = 0; i < mErrors.Length(); ++i) {
    if (mErrors[i].mErrorStart <= aOffset && mErrors[i].mErrorEnd > aOffset) {
      if (mErrors[i].mSuggestions.Length() > (int)aSuggestion) {
        *aRetVal = mErrors[i].mMessageOnly[aSuggestion];
      }
      break;
    }
  }
  

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::GetSuggestionsForOffset(nsIEditor* aEditor, uint32_t aOffset, nsIArray** aRetVal)
{
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nullptr;

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);

  if (mEditor == aEditor && mCheckEnabled) {
    for (uint32_t i = 0; i < mErrors.Length(); ++i) {
      if (mErrors[i].mErrorStart <= aOffset && mErrors[i].mErrorEnd > aOffset) {
        for (uint32_t j = 0; j < mErrors[i].mSuggestions.Length(); ++j) {
          nsCOMPtr<nsISupportsString> istr = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
          istr->SetData(mErrors[i].mSuggestions[j]);
          array->AppendElement(istr, PR_FALSE);
        }
        break;
      }
    }
  }


  array.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::GetDescriptionsForOffset(nsIEditor* aEditor, uint32_t aOffset, nsIArray** aRetVal)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aRetVal);
  *aRetVal = nullptr;

  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mEditor == aEditor && mCheckEnabled) {
    for (uint32_t i = 0; i < mErrors.Length(); ++i) {
      if (mErrors[i].mErrorStart <= aOffset && mErrors[i].mErrorEnd > aOffset) {
        for (uint32_t j = 0; j < mErrors[i].mDescriptions.Length(); ++j) {
          nsCOMPtr<nsISupportsString> istr = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
          istr->SetData(mErrors[i].mDescriptions[j]);
          array->AppendElement(istr, PR_FALSE);
        }
        break;
      }
    }
  }


  array.forget(aRetVal);

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::DoGrammarCorrection(uint32_t aOffset, uint32_t aIndex)
{
  if (mEditor && mRootNode) {
    for (uint32_t i = 0; i < mErrors.Length(); ++i) {
      if (mErrors[i].mErrorStart <= aOffset && mErrors[i].mErrorEnd > aOffset) {
        nsCOMPtr<nsISelectionController> selcon;
        mEditor->GetSelectionController(getter_AddRefs(selcon));

        nsCOMPtr<nsISelection> grammarCheckSelection;
        selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

        if (!grammarCheckSelection)
          return NS_OK;

        nsCOMPtr<nsISelectionPrivate> privSel(do_QueryInterface(grammarCheckSelection));

        nsTArray<nsRange*> ranges;

        nsCOMPtr<nsIDOMNodeList> childNodes;
        mRootNode->GetChildNodes(getter_AddRefs(childNodes));

        uint32_t childCount;
        childNodes->GetLength(&childCount);

        nsCOMPtr<nsIDOMNode> par;

        if (childCount >= 1) {
          childNodes->Item(0, getter_AddRefs(par)); 
        } else {
          return NS_OK;
        }

        nsCOMPtr<nsINode> node = do_QueryInterface(par);

        nsresult rv = privSel->GetRangesForIntervalArray(node, aOffset, node, aOffset, true, &ranges);

        nsCOMPtr<nsIDOMRange> range; 

        if (ranges.Length() == 0)
          return NS_OK; // no matches

        // there may be more than one range returned, and we don't know what do
        // do with that, so just get the first one
        NS_ADDREF(range = ranges[0]);

        if (range) {
          mEditor->BeginTransaction();

          nsCOMPtr<nsISelection> selection;
          mEditor->GetSelection(getter_AddRefs(selection));
          selection->RemoveAllRanges();
          selection->AddRange(range);
          mEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);

          nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(mEditor));
          if (textEditor && mErrors[i].mSuggestions.Length() > aIndex)
            textEditor->InsertText(mErrors[i].mSuggestions[aIndex]);

          mEditor->EndTransaction();
        }

        DoGrammarCheck();

        return NS_OK;
      }
    }
  }

  return NS_OK;
}


void nsEditorGrammarCheck::SetCurrentEditor(nsIEditor* aEditor)
{
  if (!aEditor) {
    return;
  }


  mCheckEnabled = Preferences::GetBool("grammarCheckEnabled", true);

  if (mEditor) {
    nsresult rv;

    nsCOMPtr<nsISelectionController> selcon;
    rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

    if (selcon) {
      nsCOMPtr<nsISelection> grammarCheckSelection;
      selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

      if (grammarCheckSelection)
        grammarCheckSelection->RemoveAllRanges();


      selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
    }

  }

  nsWeakPtr nwp = do_GetWeakReference(aEditor);

  mEditor = do_QueryReferent(nwp);

  DoGrammarCheck();

}


NS_IMETHODIMP nsEditorGrammarCheck::ToggleEnabled()
{
  mCheckEnabled = !mCheckEnabled;
  Preferences::SetBool("grammarCheckEnabled", mCheckEnabled);

  if (!mEditor) {
    return NS_OK;
  }

  if (!mCheckEnabled) {
    nsCOMPtr<nsISelectionController> selcon;
    nsresult rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

    if (selcon) {
      nsCOMPtr<nsISelection> grammarCheckSelection;
      selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

      if (grammarCheckSelection)
        grammarCheckSelection->RemoveAllRanges();
    }

    selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

  } else {
    DoGrammarCheck();
  }

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::IsGrammarCheckEnabled(bool* aRetVal)
{
  *aRetVal = mCheckEnabled;
  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::DoGrammarCheck()
{
  if (mEditor == nullptr || !mCheckEnabled) {
    return NS_OK;
  }

  nsresult rv;
  uint32_t aFlags = nsIDocumentEncoder::SkipInvisibleContent;

  nsCOMPtr<nsIDOMDocument> domDoc;
  rv = mEditor->GetDocument(getter_AddRefs(domDoc));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocumentEncoder> docEncoder;
  docEncoder = do_CreateInstance(NS_HTMLCOPY_ENCODER_CONTRACTID);
  NS_ENSURE_TRUE(docEncoder, NS_ERROR_FAILURE);


  // Find the root node for the editor.
  nsCOMPtr<nsIDOMElement> rootElem;
  rv = mEditor->GetRootElement(getter_AddRefs(rootElem));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElem, &rv);
  NS_ASSERTION(rootNode, "GetRootElement returned null *and* claimed to succeed!");

  if (!rootNode)
    return NS_OK;

  nsCOMPtr<nsINode> node = do_QueryInterface(rootNode);
  if (nsContentUtils::IsSystemPrincipal(node->NodePrincipal())) {
    mEditor = nullptr;
    return NS_OK;
  }

  // if it is a selection into input/textarea element or in a html content
  // with pre-wrap style : text/plain. Otherwise text/html.
  // see nsHTMLCopyEncoder::SetSelection
  nsAutoString mimeType;
  mimeType.AssignLiteral(kTextMime);

  // Do the first and potentially trial encoding as preformatted and raw.
  uint32_t flags = aFlags | nsIDocumentEncoder::OutputPreformatted
    | nsIDocumentEncoder::OutputRaw
    | nsIDocumentEncoder::OutputForPlainTextClipboardCopy
    | nsIDocumentEncoder::OutputDropInvisibleBreak;


  rv = docEncoder->Init(domDoc, mimeType, flags);
  NS_ENSURE_SUCCESS(rv, rv);


  rv = docEncoder->SetNativeContainerNode(node);
  NS_ENSURE_SUCCESS(rv, rv);
  // SetSelection set the mime type to text/plain if the selection is inside a
  // text widget.
  rv = docEncoder->GetMimeType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString buf;
  rv = docEncoder->EncodeToString(buf);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = docEncoder->GetMimeType(mimeType);
  NS_ENSURE_SUCCESS(rv, rv);

  if (gCallback)
    gCallback->DoGrammarCheck(buf);

  return NS_OK;
}


NS_GENERIC_FACTORY_SINGLETON_CONSTRUCTOR(nsEditorGrammarCheck, nsEditorGrammarCheck::GetSingleton)

// The following line defines a kNS_SAMPLE_CID CID variable.
NS_DEFINE_NAMED_CID(NS_GRAMMARCHECK_CID);

// Build a table of ClassIDs (CIDs) which are implemented by this module. CIDs
// should be completely unique UUIDs.
// each entry has the form { CID, service, factoryproc, constructorproc }
// where factoryproc is usually NULL.
static const mozilla::Module::CIDEntry kSampleCIDs[] = {
    { &kNS_GRAMMARCHECK_CID, false, nullptr, nsEditorGrammarCheckConstructor },
    { nullptr }
};

// Build a table which maps contract IDs to CIDs.
// A contract is a string which identifies a particular set of functionality. In some
// cases an extension component may override the contract ID of a builtin gecko component
// to modify or extend functionality.
static const mozilla::Module::ContractIDEntry kSampleContracts[] = {
    { NS_GRAMMARCHECK_CONTRACTID, &kNS_GRAMMARCHECK_CID },
    { nullptr }
};

// Category entries are category/key/value triples which can be used
// to register contract ID as content handlers or to observe certain
// notifications. Most modules do not need to register any category
// entries: this is just a sample of how you'd do it.
// @see nsICategoryManager for information on retrieving category data.
static const mozilla::Module::CategoryEntry kSampleCategories[] = {
    { "my-category", "my-key", NS_GRAMMARCHECK_CONTRACTID },
    { nullptr }
};

static const mozilla::Module kSampleModule = {
  mozilla::Module::kVersion,
  kSampleCIDs,
  kSampleContracts,
  kSampleCategories
};

// The following line implements the one-and-only "NSModule" symbol exported from this
// shared library.
NSMODULE_DEFN(nsGrammarCheckModule) = &kSampleModule;

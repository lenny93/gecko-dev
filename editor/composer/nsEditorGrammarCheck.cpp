#include <iostream>                 // for getenv
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
#include "nsIPlaintextEditor.h"
#include "../../modules/libpref/Preferences.h"

using namespace mozilla;

GC_FACTORY_SINGLETON_IMPLEMENTATION(nsEditorGrammarCheck, gGrammarCheckService)
NS_IMPL_CLASSINFO(nsEditorGrammarCheck, nullptr, 0, NS_GRAMMARCHECK_CID)
NS_IMPL_ISUPPORTS_CI(nsEditorGrammarCheck, nsIEditorGrammarCheck)


nsEditorGrammarCheck::nsEditorGrammarCheck() : gCallback(nullptr), mEditor(), mCheckEnabled(true)
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

NS_IMETHODIMP nsEditorGrammarCheck::Init()
{
	mCheckEnabled = Preferences::GetBool("grammarCheckEnabled", true);
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

	nsCOMPtr<nsIDOMNode> rootNode = do_QueryInterface(rootElem);
	mRootNode = rootNode;
	NS_ASSERTION(mRootNode, "GetRootElement returned null *and* claimed to suceed!");

	if (!mRootNode)
		return NS_OK;

	nsCOMPtr<nsISelectionController> selcon;
	rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

	nsCOMPtr<nsISelection> grammarCheckSelection;
	//selcon->SetDisplaySelection(nsISelectionController::SELECTION_GRAMMARCHECK);
	selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

	if (!grammarCheckSelection)
		return NS_OK;

	grammarCheckSelection->RemoveAllRanges();
	

	for (int i = 0; i < count; i++)
	{
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

		if (aEndOffset)
			range->SetEnd(aEndNode, aEndOffset);
		else
			range->SetEndAfter(aEndNode);

		//nsRange* mRange = static_cast<nsRange*>(range.forget().take());


		grammarCheckSelection->AddRange(range);

		GRAMMARERROR ge;
		ge.errorStart = errorsStart[i];
		ge.errorEnd = errorsEnd[i];

		mErrors.push_back(ge);
	}


	selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

    return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::AddSuggestionForError(uint32_t error, const nsAString& suggestion, const nsAString& description)
{
	if (mErrors.size() <= (int)error)
		return NS_OK;

	

	mErrors[error].suggestions.push_back(nsString(suggestion));
	mErrors[error].descriptions.push_back(nsString(description));
	
	return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::GetSuggestionsForOffset(nsIEditor* editor, uint32_t aOffset, nsIArray** _retval)
{
	nsresult rv;
	NS_ENSURE_ARG_POINTER(_retval);
	*_retval = nullptr;

	nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID);

	if (mEditor == editor && mCheckEnabled)
	{
		for (int i = 0; i < mErrors.size(); ++i)
		{
			if (mErrors[i].errorStart <= aOffset && mErrors[i].errorEnd > aOffset)
			{
				for (int j = 0; j < mErrors[i].suggestions.size(); ++j)
				{
					nsCOMPtr<nsISupportsString> istr = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
					istr->SetData(mErrors[i].suggestions[j]);
					array->AppendElement(istr, PR_FALSE);
				}
				break;
			}
		}
	}


	array.forget(_retval);

	return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::GetDescriptionsForOffset(nsIEditor* editor, uint32_t aOffset, nsIArray** _retval)
{
	nsresult rv;
	NS_ENSURE_ARG_POINTER(_retval);
	*_retval = nullptr;

	nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
	NS_ENSURE_SUCCESS(rv, rv);

	if (mEditor == editor && mCheckEnabled)
	{
		for (int i = 0; i < mErrors.size(); ++i)
		{
			if (mErrors[i].errorStart <= aOffset && mErrors[i].errorEnd > aOffset)
			{
				for (int j = 0; j < mErrors[i].descriptions.size(); ++j)
				{
					nsCOMPtr<nsISupportsString> istr = do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
					istr->SetData(mErrors[i].descriptions[j]);
					array->AppendElement(istr, PR_FALSE);
				}
				break;
			}
		}
	}


	array.forget(_retval);

	return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::DoGrammarCorrection(uint32_t aOffset, uint32_t index)
{
	if (mEditor && mRootNode)
	{
		for (int i = 0; i < mErrors.size(); ++i)
		{
			if (mErrors[i].errorStart <= aOffset && mErrors[i].errorEnd > aOffset)
			{
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

				if (childCount >= 1)
				{
					childNodes->Item(0, getter_AddRefs(par));
				}
				else
				{
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

				if (range)
				{
					mEditor->BeginTransaction();

					nsCOMPtr<nsISelection> selection;
					mEditor->GetSelection(getter_AddRefs(selection));
					selection->RemoveAllRanges();
					selection->AddRange(range);
					mEditor->DeleteSelection(nsIEditor::eNone, nsIEditor::eStrip);

					nsCOMPtr<nsIPlaintextEditor> textEditor(do_QueryInterface(mEditor));
					if (textEditor && mErrors[i].suggestions.size() > index)
					{
						textEditor->InsertText(mErrors[i].suggestions[index]);
					}

					mEditor->EndTransaction();
				}

				DoGrammarCheck();

				return NS_OK;
			}
		}
	}

	return NS_OK;
}


void nsEditorGrammarCheck::SetCurrentEditor(nsIEditor* editor)
{
	if (!editor)
		return;


	mCheckEnabled = Preferences::GetBool("grammarCheckEnabled", true);

	if (mEditor)
	{
		nsresult rv;

		nsCOMPtr<nsISelectionController> selcon;
		rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

		if (selcon)
		{
			nsCOMPtr<nsISelection> grammarCheckSelection;
			//selcon->SetDisplaySelection(nsISelectionController::SELECTION_GRAMMARCHECK);
			selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

			if (grammarCheckSelection)
				grammarCheckSelection->RemoveAllRanges();


			selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);
		}

	}

	nsWeakPtr nwp = do_GetWeakReference(editor);

	mEditor = do_QueryReferent(nwp);

	DoGrammarCheck();

}

NS_IMETHODIMP nsEditorGrammarCheck::ToggleEnabled()
{
	mCheckEnabled = !mCheckEnabled;
	Preferences::SetBool("grammarCheckEnabled", mCheckEnabled);

	if (!mEditor)
		return NS_OK;

	if (!mCheckEnabled)
	{
		nsresult rv;

		nsCOMPtr<nsISelectionController> selcon;
		rv = mEditor->GetSelectionController(getter_AddRefs(selcon));

		if (selcon)
		{
			nsCOMPtr<nsISelection> grammarCheckSelection;
			//selcon->SetDisplaySelection(nsISelectionController::SELECTION_GRAMMARCHECK);
			selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));

			if (grammarCheckSelection)
				grammarCheckSelection->RemoveAllRanges();
		}

		selcon->RepaintSelection(nsISelectionController::SELECTION_NORMAL);

	}
	else
	{
		DoGrammarCheck();
	}

	return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::IsGrammarCheckEnabled(bool* _retval)
{
	*_retval = mCheckEnabled;
	return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::DoGrammarCheck()
{
	if (mEditor == nullptr || !mCheckEnabled)
		return NS_OK;

	nsresult rv;

	nsString os;
	mEditor->OutputToString(NS_LITERAL_STRING("text/plain"), 4, os);

	if (NS_ConvertUTF16toUTF8(os).get() == "")
		return NS_OK;


	mErrors.clear();

	if (gCallback)
		gCallback->DoGrammarCheck(os);


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

// The following line implements the one-and-only "NSGetModule" symbol
// for compatibility with mozilla 1.9.2. You should only use this
// if you need a binary which is backwards-compatible and if you use
// interfaces carefully across multiple versions.
//NS_IMPL_MOZILLA192_NSGETMODULE(&kSampleModule)
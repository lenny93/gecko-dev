#include "nsEditorGrammarCheck.h"
#include "..\libeditor\nsEditor.h"
#include "mozilla/dom/Selection.h"
#include "nsMemory.h"
#include "nsIClassInfoImpl.h"
#include "mozilla/ModuleUtils.h"
#include <string.h>                     // for nullptr, strcmp
#include <windows.h>
#include <iostream>

using namespace mozilla;

GC_FACTORY_SINGLETON_IMPLEMENTATION(nsEditorGrammarCheck, gGrammarCheckService)
NS_IMPL_CLASSINFO(nsEditorGrammarCheck, nullptr, 0, NS_GRAMMARCHECK_CID)
NS_IMPL_ISUPPORTS_CI(nsEditorGrammarCheck, nsIEditorGrammarCheck)


nsEditorGrammarCheck::nsEditorGrammarCheck() : gCallback(nullptr), currentEditor(nullptr)
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

nsresult nsEditorGrammarCheck::Init()
{

  return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::Poke(const char* aValue)
{
	std::cout << "Calling from addon: " << aValue << std::endl;
    return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::RegisterAddon(nsIEditorGrammarCheckCallback* callback)
{
	gCallback = callback;
    return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::ErrorsFound(uint32_t count, uint32_t* errors)
{
	for (int i = 0; i < count; i++)
        std::cout << "Error number " << errors[i] << std::endl; 
	
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


void nsEditorGrammarCheck::SetCurrentEditor(nsEditor* editor)
{
	mEditor = editor;

	//nsCOMPtr<nsIEditor> editor = do_QueryReferent(mEditor);
	if (!editor)
		return; // editor is gone

	//nsCOMPtr<nsIEditor> editor(do_QueryReferent(mEditor));
	//NS_ENSURE_TRUE(editor, NS_ERROR_NULL_POINTER);

	nsCOMPtr<nsISelectionController> selcon;
	nsresult rv = editor->GetSelectionController(getter_AddRefs(selcon));
	//NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsISelection> grammarCheckSelection;
	selcon->GetSelection(nsISelectionController::SELECTION_GRAMMARCHECK, getter_AddRefs(grammarCheckSelection));


	//nsCOMPtr<nsISelection> spellCheckSelectionRef;
	//rv = GetSpellCheckSelection(getter_AddRefs(spellCheckSelectionRef));
	//NS_ENSURE_SUCCESS(rv, rv);

	//auto spellCheckSelection = static_cast<Selection *>(spellCheckSelectionRef.get());

	//nsAutoString currentDictionary;
	//rv = mSpellCheck->GetCurrentDictionary(currentDictionary);
	//if (NS_FAILED(rv)) {
	//	// no active dictionary
	//	int32_t count = spellCheckSelection->RangeCount();
	//	for (int32_t index = count - 1; index >= 0; index--) {
	//		nsRange *checkRange = spellCheckSelection->GetRangeAt(index);
	//		if (checkRange) {
	//			RemoveRange(spellCheckSelection, checkRange);
	//		}
	//	}
	//	return NS_OK;
	//}

	//CleanupRangesInSelection(spellCheckSelection);

	//rv = aStatus->FinishInitOnEvent(wordUtil);
	//NS_ENSURE_SUCCESS(rv, rv);
	//if (!aStatus->mRange)
	//	return NS_OK; // empty range, nothing to do

	//bool doneChecking = true;
	//if (aStatus->mOp == mozInlineSpellStatus::eOpSelection)
	//	rv = DoSpellCheckSelection(wordUtil, spellCheckSelection, aStatus);
	//else
	//	rv = DoSpellCheck(wordUtil, spellCheckSelection, aStatus, &doneChecking);
	//NS_ENSURE_SUCCESS(rv, rv);

}
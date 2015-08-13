#include <stdio.h>
#include "nsEditorGrammarCheck.h"

#include "nsMemory.h"
#include "nsEmbedString.h"
#include "nsIClassInfoImpl.h"

NS_IMPL_ISUPPORTS1(nsEditorGrammarCheck, nsIEditorGrammarCheck)

nsEditorGrammarCheck::nsEditorGrammarCheck() : mValue(nullptr)
{
    mValue = (char*)nsMemory::Clone("initial value", 14);
}

nsEditorGrammarCheck::~nsEditorGrammarCheck()
{
    if (mValue)
        nsMemory::Free(mValue);
}


NS_IMPL_CLASSINFO(nsEditorGrammarCheck, NULL, 0, NS_GRAMMARCHECK_CID)
NS_IMPL_ISUPPORTS1_CI(nsEditorGrammarCheck, nsIEditorGrammarCheck)

/**
 * Notice that in the protoype for this function, the NS_IMETHOD macro was
 * used to declare the return type.  For the implementation, the return
 * type is declared by NS_IMETHODIMP
 */
NS_IMETHODIMP nsEditorGrammarCheck::GetValue(char** aValue)
{
    NS_PRECONDITION(aValue != nullptr, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        /**
         * GetValue's job is to return data known by an instance of
         * nsSampleImpl to the outside world.  If we  were to simply return 
         * a pointer to data owned by this instance, and the client were to
         * free it, bad things would surely follow.
         * On the other hand, if we create a new copy of the data for our
         * client, and it turns out that client is implemented in JavaScript,
         * there would be no way to free the buffer.  The solution to the 
         * buffer ownership problem is the nsMemory singleton.  Any buffer
         * returned by an XPCOM method should be allocated by the nsMemory.
         * This convention lets things like JavaScript reflection do their
         * job, and simplifies the way C++ clients deal with returned buffers.
         */
        *aValue = (char*) nsMemory::Clone(mValue, strlen(mValue) + 1);
        if (! *aValue)
            return NS_ERROR_NULL_POINTER;
    }
    else {
        *aValue = nullptr;
    }
    return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::SetValue(const char* aValue)
{
    NS_PRECONDITION(aValue != nullptr, "null ptr");
    if (! aValue)
        return NS_ERROR_NULL_POINTER;

    if (mValue) {
        nsMemory::Free(mValue);
    }

    /**
     * Another buffer passing convention is that buffers passed INTO your
     * object ARE NOT YOURS.  Keep your hands off them, unless they are
     * declared "inout".  If you want to keep the value for posterity,
     * you will have to make a copy of it.
     */
    mValue = (char*) nsMemory::Clone(aValue, strlen(aValue) + 1);
    return NS_OK;
}

NS_IMETHODIMP nsEditorGrammarCheck::Poke(const char* aValue)
{
    return SetValue((char*) aValue);
}


static void nsEditorGrammarCheck(nsACString& aValue)
{
    NS_CStringSetData(aValue, "GetValue");
}

NS_IMETHODIMP nsEditorGrammarCheck::WriteValue(const char* aPrefix)
{
    NS_PRECONDITION(aPrefix != nullptr, "null ptr");
    if (! aPrefix)
        return NS_ERROR_NULL_POINTER;

    printf("%s %s\n", aPrefix, mValue);

    // This next part illustrates the nsEmbedString:
    nsEmbedString foopy;
    foopy.Append(PRUnichar('f'));
    foopy.Append(PRUnichar('o'));
    foopy.Append(PRUnichar('o'));
    foopy.Append(PRUnichar('p'));
    foopy.Append(PRUnichar('y'));
    
    const PRUnichar* f = foopy.get();
    uint32_t l = foopy.Length();
    printf("%c%c%c%c%c %d\n", char(f[0]), char(f[1]), char(f[2]), char(f[3]), char(f[4]), l);
    
    nsEmbedCString foopy2;
    GetStringValue(foopy2);

    //foopy2.AppendLiteral("foopy");
    const char* f2 = foopy2.get();
    uint32_t l2 = foopy2.Length();

    printf("%s %d\n", f2, l2);

    return NS_OK;
}

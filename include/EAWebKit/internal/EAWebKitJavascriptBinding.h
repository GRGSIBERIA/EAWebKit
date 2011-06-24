/*
Copyright (C) 2008-2009 Electronic Arts, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1.  Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
3.  Neither the name of Electronic Arts, Inc. ("EA") nor the names of
its contributors may be used to endorse or promote products derived
from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ELECTRONIC ARTS AND ITS CONTRIBUTORS "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ELECTRONIC ARTS OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

///////////////////////////////////////////////////////////////////////////////
// EAWebKitJavascriptBinding.h
//
// By Chris Stott
///////////////////////////////////////////////////////////////////////////////

#ifndef EAWEBKIT_EAWEBKITJAVASCRIPTBINDING_H
#define EAWEBKIT_EAWEBKITJAVASCRIPTBINDING_H

#include <EAWebKit/EAWebKitJavascriptValue.h>

#if EAWEBKIT_THROW_BUILD_ERROR
#error This file should be included only in a dll build
#endif

namespace EA
{
	namespace WebKit
	{
		
		//////////////////////////////////////////////////////////////////////////
		//
		class JavascriptBindingObject : public BalObject
		{
		public:
			//////////////////////////////////////////////////////////////////////////
			//
			JavascriptBindingObject(ViewNotification* viewNotification)
			:	mViewNotification(viewNotification)
			{
			}

			//////////////////////////////////////////////////////////////////////////
			//
			~JavascriptBindingObject()
			{
			}

			//////////////////////////////////////////////////////////////////////////
			//
			BalValue* invoke( const char *name, Vector<BalValue*> args)
			{
				if (mViewNotification)
				{
					JavascriptMethodInvokedInfo info;
					info.mMethodName = name;

					unsigned counter = 0;

					for (Vector<BalValue*>::const_iterator i = args.begin(); i != args.end(); ++i)
					{
						info.mArguments[counter++] = Translate(*i);
					}
					info.mArgumentCount = counter;


					mViewNotification->JavascriptMethodInvoked(info);

					return Translate(info.mReturn);
				}
				else
				{
					BalValue* value = &mValue;
					value->balUndefined();
					return value;
				}
				
			}

			//////////////////////////////////////////////////////////////////////////
			//
			BalValue *getProperty(const char *name)
			{
				if (mViewNotification)
				{
					JavascriptPropertyInfo info;
					info.mPropertyName = name;
					mViewNotification->GetJavascriptProperty(info);
					return Translate(info.mValue);
				}
				else
				{
					BalValue* value = &mValue;
					value->balUndefined();
					return value;
				}
			};

			//////////////////////////////////////////////////////////////////////////
			//
			void setProperty( const char *name, const BalValue *value) 
			{
				if (mViewNotification)
				{
					JavascriptPropertyInfo info;
					info.mPropertyName = name;
					info.mValue = Translate(value);
					mViewNotification->SetJavascriptProperty(info);
				}
			};


		private:
			//////////////////////////////////////////////////////////////////////////
			// object marshalling 
			BalValue* Translate(JavascriptValue& value)
			{
				BalValue* translated = &mValue;

				switch (value.GetType())
				{
				case JavascriptValueType_Undefined:
					translated->balUndefined();
					break;	
				case JavascriptValueType_Boolean:
					translated->balBoolean(value.GetBooleanValue());
					break;
				case JavascriptValueType_Number:
					translated->balNumber(value.GetNumberValue());
					break;
				case JavascriptValueType_String:
					translated->balString(value.GetStringValue());
					break;

				}

				return translated;
			}

			//////////////////////////////////////////////////////////////////////////
			//
			JavascriptValue Translate(const BalValue* value)
			{
				JavascriptValue translated;

				switch (value->type())
				{
				case BooleanType:
					translated.SetBooleanValue(value->toBoolean());
					break;
				case UndefinedType:
					translated.SetUndefined();
					break;
				case NumberType:
					translated.SetNumberValue(value->toNumber());
					break;
				case StringType:
					mString = value->toString();
					translated.SetStringValue(mString.charactersWithNullTermination());


					break;
				default:
					// maybe should assert?
					break;
				}

				return translated;
			}

			ViewNotification*	mViewNotification;
			BalValue			mValue;
			WebCore::String		mString;
		};

	} // namespace WebKit
} // namespace EA

#endif //EAWEBKIT_EAWEBKITJAVASCRIPTBINDING_H

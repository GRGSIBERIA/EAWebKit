/*
Copyright (C) 2009 Electronic Arts, Inc.  All rights reserved.

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
// EAWebKitDocumentNavigator.cpp
// By Chris Stott
///////////////////////////////////////////////////////////////////////////////

#include <EAWebKit/internal/InputBinding/EAWebKitDocumentNavigator.h>
#include <EAWebKit/internal/InputBinding/EAWebKitPolarRegion.h>
#include <EAWebKit/internal/InputBinding/EAWebKitUtils.h>
#include <NodeList.h>
#include <EventNames.h>
#include <HTMLInputElement.h>
#include <platform/graphics/IntRect.h>
#include <EAWebKit/internal/EAWebKitAssert.h>
#include <EAWebKit/internal/EAWebKitEASTLHelpers.h>
#include <EAWebKit/internal/EAWebkitNodeListContainer.h>
#include "RenderStyle.h"

namespace EA
{
	namespace WebKit
	{
		DocumentNavigator::DocumentNavigator(EA::WebKit::JumpDirection direction, WebCore::IntPoint startingPosition, int previousNodeX, int previousNodeY, int previousNodeWidth, int previousNodeHeight, float theta)
			:	mDirection(direction)
			,	mStartingPosition(startingPosition)
			,	mPreviousNodeRect(previousNodeX,previousNodeY,previousNodeWidth,previousNodeHeight)
			,	mBestNode(0)
			,	mMinR(10000000.0f)
			,	mNodeListContainer(0)
			
		{

			mNodeListContainer = WTF::fastNew<NodeListContainer> ();

			int width = previousNodeWidth;
			int height = previousNodeHeight;

			switch (direction)
			{
			case EA::WebKit::JumpRight:
				mStartingPosition.setX(mStartingPosition.x() + 3*width/8);
				break;

			case EA::WebKit::JumpDown:
				mStartingPosition.setY(mStartingPosition.y() + 3*height/8);
				break;

			case EA::WebKit::JumpLeft:
				mStartingPosition.setX(mStartingPosition.x() - 3*width/8);
				break;

			case EA::WebKit::JumpUp:
				mStartingPosition.setY(mStartingPosition.y() - 3*height/8);
				break;

			default:
				EAW_FAIL_MSG("Should not have got here\n");
			}

			mAxesX = mStartingPosition.x();
			mAxesY = mStartingPosition.y();

			const float PI_4 = 3.14159f / 4.0f;

			switch (direction)
			{
			case EA::WebKit::JumpRight:
				mMinThetaRange = 8*PI_4 - theta;
				mMaxThetaRange = theta;



				//printf("JumpingRight : theta=[%f,%f]\n", mMinThetaRange, mMaxThetaRange);

				break;

			case EA::WebKit::JumpDown:
				mMinThetaRange = 2*PI_4-theta;
				mMaxThetaRange = 2*PI_4+theta;



				//printf("JumpingDown : theta=[%f,%f]\n", mMinThetaRange, mMaxThetaRange);
				break;

			case EA::WebKit::JumpLeft:
				mMinThetaRange = 4*PI_4 - theta;
				mMaxThetaRange = 4*PI_4 + theta;


				//printf("JumpingLeft : theta=[%f,%f]\n", mMinThetaRange, mMaxThetaRange);
				break;

			case EA::WebKit::JumpUp:
				mMinThetaRange = 6*PI_4 - theta;
				mMaxThetaRange = 6*PI_4 + theta;


				//printf("JumpingUp : theta=[%f,%f]\n", mMinThetaRange, mMaxThetaRange);
				break;

			default:
				EAW_FAIL_MSG("Should not have got here\n");
			}


			//printf("atan2(1.0f,0.0f)=%f [+XAXIS]\n",		CalcAngle(1.0f,0.0f) );
			//printf("atan2(0.0f,1.0f)=%f [+YAXIS]\n",		CalcAngle(0.0f,1.0f) );
			//printf("atan2(-1.0f,0.0f)=%f [-XAXIS]\n",		CalcAngle(-1.0f,0.0f) );
			//printf("atan2(0.0f,-1.0f)=%f [-YAXIS]\n",		CalcAngle(0.0f,-1.0f) );

			//printf("atan2(1.0f,1.0f)=%f [DIAGONAL1]\n",	CalcAngle(1.0f,1.0f) );
			//printf("atan2(-1.0f,1.0f)=%f [DIAGONAL2]\n",	CalcAngle(-1.0f,1.0f) );
			//printf("atan2(-1.0f,-1.0f)=%f [DIAGONAL3]\n", CalcAngle(-1.0f,-1.0f) );
			//printf("atan2(1.0f,-1.0f)=%f [DIAGONAL4]\n",	CalcAngle(1.0f,-1.0f) );
		}

		DocumentNavigator::~DocumentNavigator()
		{
			if(mNodeListContainer)
			{
				WTF::fastDelete<NodeListContainer>(mNodeListContainer);
				mNodeListContainer = 0;
			}
		}
		static bool WouldBeTrappedInElement(const WebCore::IntRect& rect, const WebCore::IntPoint& point, EA::WebKit::JumpDirection direction)
		{
			// If we're not inside, don't worry
			if (rect.contains(point))
			{
				typedef WebCore::IntPoint Vector2D;

				const WebCore::IntPoint centre = Average( rect.bottomLeft(), rect.topRight() );

				Vector2D pointToCentre = Subtract(centre,point);
				Vector2D forward;

				switch (direction)
				{
				// note these are 'backward
				case EA::WebKit::JumpUp:		forward = Vector2D(0,-100); break;
				case EA::WebKit::JumpDown:		forward = Vector2D(0,100); break;
				case EA::WebKit::JumpLeft:		forward = Vector2D(-100,0); break;
				case EA::WebKit::JumpRight:		forward = Vector2D(100,0); break;
				}

				// Basically, if the centre is behind us, don't jump there
				if (DotProduct(forward,pointToCentre) < 0)
				{
					return false;
				}
				else
				{
					/*printf("(%d,%d) Trapped Inside Element (%d,%d)->(%d,%d): forward=(%d,%d) pointToCentre=(%d,%d) dot=%d\n",
						point.x(),
						point.y(),
						rect.bottomLeft().x(),
						rect.bottomLeft().y(),
						rect.topRight().x(),
						rect.topRight().y(),
						forward.x(),
						forward.y(),
						pointToCentre.x(),
						pointToCentre.y(),
						DotProduct(forward,pointToCentre)
						);*/
					return true;
				}
			}
			else
			{
				return false;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		//
		static bool TryingToDoPerpendicularJump(const WebCore::IntRect& rect, const WebCore::IntRect& previousRect, EA::WebKit::JumpDirection direction)
		{
			if (direction == EA::WebKit::JumpLeft || direction == EA::WebKit::JumpRight)
			{
				if (rect.topRight().x() <= previousRect.topRight().x() && rect.topLeft().x() >= previousRect.topLeft().x())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				if (rect.topRight().y() <= previousRect.topRight().y() && rect.bottomRight().y() >= previousRect.bottomRight().y())
				{
					return true;
				}
				else
				{
					return false;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		//
		void DocumentNavigator::FindBestNode(WebCore::Node* rootNode)
		{
			//////////////////////////////////////////////////////////////////////////
			// WORRY ABOUT YOURSELF 
			if (rootNode->nodeType() == WebCore::Node::ELEMENT_NODE)
			{
				WebCore::Element* element = (WebCore::HTMLElement*)rootNode;
				if (element->isHTMLElement())
				{
					WebCore::HTMLElement* htmlElement = (WebCore::HTMLElement*)element;

					if (!htmlElement->hasClass() || !htmlElement->classNames().contains("navigation_ignore"))
					{
						if (htmlElement->tagName()=="A" || htmlElement->tagName()=="INPUT" || htmlElement->tagName()=="TEXTAREA")
						{
							if (htmlElement->computedStyle()->visibility()==WebCore::VISIBLE)
							{
								if (htmlElement->computedStyle()->display()!=WebCore::NONE)
					            {
						            WebCore::IntRect rect = htmlElement->getRect();					
            
						            /* printf("Looking at ELEMENT_NODE : nodeName=%S (%d,%d)->(%d,%d) ThetaRange(%f,%f)\n\n%S\n-----------------------------------\n", 
							            htmlElement->tagName().charactersWithNullTermination(),
							            rect.topLeft().x(),rect.topLeft().y(),
							            rect.bottomRight().x(), rect.bottomRight().y(),
							            mMinThetaRange,mMaxThetaRange,
							            htmlElement->innerHTML().charactersWithNullTermination()
							            );
									*/

						            if (!WouldBeTrappedInElement(rect,mStartingPosition,mDirection))
						            {
							            if (!TryingToDoPerpendicularJump(rect,mPreviousNodeRect,mDirection))
							            {
								            if(rect.width()>5 && rect.height() > 5)
								            {
									            if (doAxisCheck(rect))
									            {
										            PolarRegion pr(htmlElement->getRect(), mStartingPosition);
            
										            if (pr.minR < mMinR )
										            {
											            if (areAnglesInRange(pr.minTheta,pr.maxTheta))
											            {
												            mMinR = pr.minR;
            
												            EAW_ASSERT( *(uint32_t*)rootNode > 10000000u );
            
												            mBestNode = rootNode;
												            mNodeListContainer->mFoundNodes.push_back(rootNode);
												            /*printf("Found ELEMENT_NODE : nodeName=%s (%d,%d)->(%d,%d) polar: R(%f,%f) Theta(%f,%f) ThetaRange(%f,%f)  \n", 
												            (char*)htmlElement->nodeName().characters(),
												            rect.topLeft().x(),rect.topLeft().y(),
												            rect.bottomRight().x(), rect.bottomRight().y(),
												            pr.minR,pr.maxR,pr.minTheta,pr.maxTheta,
												            mMinThetaRange,mMaxThetaRange
												            );*/
											            }
											            else
											            {
												            mNodeListContainer->mRejectedByAngleNodes.push_back(rootNode);
															/*printf("RejectedA ELEMENT_NODE : nodeName=%s (%d,%d)->(%d,%d) polar: R(%f,%f) Theta(%f,%f) ThetaRange(%f,%f)  \n", 
												            (char*)htmlElement->nodeName().characters(),
												            rect.topLeft().x(),rect.topLeft().y(),
												            rect.bottomRight().x(), rect.bottomRight().y(),
												            pr.minR,pr.maxR,pr.minTheta,pr.maxTheta,
												            mMinThetaRange,mMaxThetaRange
												            );*/
											            }
										            }
										            else
										            {
											            mNodeListContainer->mRejectedByRadiusNodes.push_back(rootNode);
											            /*printf("RejectedR ELEMENT_NODE : nodeName=%s (%d,%d)->(%d,%d) polar: R(%f,%f) Theta(%f,%f) ThetaRange(%f,%f)  \n", 
											            (char*)htmlElement->nodeName().characters(),
											            rect.topLeft().x(),rect.topLeft().y(),
											            rect.bottomRight().x(), rect.bottomRight().y(),
											            pr.minR,pr.maxR,pr.minTheta,pr.maxTheta,
											            mMinThetaRange,mMaxThetaRange
											            );*/
										            }
									            }
												else
												{
													//printf(" - failed axis check\n");
												}
											}
											else
											{
												//printf(" - too small\n");
								            }
										}
										else 
										{
											//printf(" - perpendicular\n");
										}							
						            }
						            else
						            {
							            mNodeListContainer->mRejectedWouldBeTrappedNodes.push_back(rootNode);
						            }
								}
							}
						}
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// THEN, FIND THE CHILDREN
			if (rootNode && rootNode->childNodeCount() > 0)
			{
				PassRefPtr<WebCore::NodeList> children = rootNode->childNodes();

				const uint32_t length = children->length();

				for (uint32_t i=0; i < length; ++i)
				{
					WebCore::Node* child = children->item(i);
					if (child)
					{
						FindBestNode(child);
					}
				}
			}
		}

		bool DocumentNavigator::doAxisCheck(WebCore::IntRect rect)
		{
			
			int left = rect.x();
			int right = rect.x() + rect.width();
			int top = rect.y();
			int bottom = rect.y() + rect.height();

			switch (mDirection)
			{
			case EA::WebKit::JumpRight:
				return !(left<mStartingPosition.x() && right<mStartingPosition.x());
				break;

			case EA::WebKit::JumpDown:
				return !(top<mStartingPosition.y() && bottom<mStartingPosition.y());
				break;

			case EA::WebKit::JumpLeft:
				return !(left>mStartingPosition.x() && right>mStartingPosition.x());
				break;

			case EA::WebKit::JumpUp:
				return !(top>mStartingPosition.y() && bottom>mStartingPosition.y());
				break;

			default:
				EAW_FAIL_MSG("Should not have got here\n");
			}

			return false;
		}

		bool DocumentNavigator::areAnglesInRange(float minTheta, float maxTheta)
		{
			float tempMaxThetaRange;
			float tempMinThetaRange;
			float tempMaxTheta;
			float tempMinTheta;

			const float TWO_PI = 2*3.14159f;

			if (mMinThetaRange < mMaxThetaRange)
			{
				// We're in a normal 
				if (minTheta < maxTheta)
				{
					tempMaxThetaRange = mMaxThetaRange;
					tempMinThetaRange = mMinThetaRange;
					tempMaxTheta = maxTheta;
					tempMinTheta = minTheta;
				}
				else
				{
					tempMaxThetaRange = mMaxThetaRange;
					tempMinThetaRange = mMinThetaRange;
					tempMaxTheta = maxTheta + TWO_PI;
					tempMinTheta = minTheta;
				}

			}
			else
			{

				// We're crossing a cut line (at 2pi)
				if (minTheta < maxTheta)
				{
					tempMaxThetaRange = mMaxThetaRange + TWO_PI;
					tempMinThetaRange = mMinThetaRange;
					tempMaxTheta = maxTheta;
					tempMinTheta = minTheta;
				}
				else
				{
					tempMaxThetaRange = mMaxThetaRange + TWO_PI;
					tempMinThetaRange = mMinThetaRange;
					tempMaxTheta = maxTheta + TWO_PI;
					tempMinTheta = minTheta;
				}

			}


			bool onOneSide = tempMinTheta>tempMaxThetaRange && tempMaxTheta>tempMaxThetaRange;
			bool onOtherSide = tempMinTheta<tempMinThetaRange && tempMaxTheta<tempMinThetaRange;

			return !(onOneSide || onOtherSide);

		}


	} // namespace WebKit
} // namespace EA
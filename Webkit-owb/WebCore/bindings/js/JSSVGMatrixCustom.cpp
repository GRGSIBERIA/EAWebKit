/*
 * Copyright (C) 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#if ENABLE(SVG)

#include "JSSVGMatrix.h"

#include "AffineTransform.h"
#include "SVGException.h"

using namespace KJS;

namespace WebCore {

JSValue* JSSVGMatrix::multiply(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    AffineTransform secondMatrix = toSVGMatrix(args[0]);    
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.multiply(secondMatrix)).get(), m_context.get());
}

JSValue* JSSVGMatrix::inverse(ExecState* exec, const ArgList&)
{
    AffineTransform imp(*impl());
    KJS::JSValue* result = toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.inverse()).get(), m_context.get());

    if (!imp.isInvertible())
        setDOMException(exec, SVGException::SVG_MATRIX_NOT_INVERTABLE);

    return result;
}

JSValue* JSSVGMatrix::translate(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float x = args[0]->toFloat(exec);
    float y = args[1]->toFloat(exec);

    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.translate(x, y)).get(), m_context.get());
}

JSValue* JSSVGMatrix::scale(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float scaleFactor = args[0]->toFloat(exec);
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.scale(scaleFactor)).get(), m_context.get());
}

JSValue* JSSVGMatrix::scaleNonUniform(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float scaleFactorX = args[0]->toFloat(exec);
    float scaleFactorY = args[1]->toFloat(exec);

    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.scaleNonUniform(scaleFactorX, scaleFactorY)).get(), m_context.get());
}

JSValue* JSSVGMatrix::rotate(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float angle = args[0]->toFloat(exec);
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.rotate(angle)).get(), m_context.get());
}

JSValue* JSSVGMatrix::rotateFromVector(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());
 
    float x = args[0]->toFloat(exec);
    float y = args[1]->toFloat(exec);

    KJS::JSValue* result = toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.rotateFromVector(x, y)).get(), m_context.get());

    if (x == 0.0 || y == 0.0)
        setDOMException(exec, SVGException::SVG_INVALID_VALUE_ERR);

    return result;
}

JSValue* JSSVGMatrix::flipX(ExecState* exec, const ArgList&)
{
    AffineTransform imp(*impl());
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.flipX()).get(), m_context.get());
}

JSValue* JSSVGMatrix::flipY(ExecState* exec, const ArgList&)
{
    AffineTransform imp(*impl());
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.flipY()).get(), m_context.get());
}

JSValue* JSSVGMatrix::skewX(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float angle = args[0]->toFloat(exec);
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.skewX(angle)).get(), m_context.get());
}

JSValue* JSSVGMatrix::skewY(ExecState* exec, const ArgList& args)
{
    AffineTransform imp(*impl());

    float angle = args[0]->toFloat(exec);
    return toJS(exec, JSSVGPODTypeWrapperCreatorReadOnly<AffineTransform>::create(imp.skewY(angle)).get(), m_context.get());
}

}

#endif // ENABLE(SVG)

// vim:ts=4:noet

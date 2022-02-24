// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

namespace nx::reflect {

/**
 * Allows context to be held during structure traversal.
 * Example of Visitor
 * <pre><code>
 * class Visitor:
 *     public GenericVisitor<Visitor>
 * {
 * public:
 *     Visitor();
 *
 *     // Called for each field. (Required method)
 *     template<typename WrappedField>
 *     void visitField(const WrappedField& field);
 *
 *     // Called after traversing all fields.
 *     // Returned value is then returned by GenericVisitor::visitAllFields.
 *     Field_traversal_result_type finish();
 * };
 * </code></pre>
 *
 * Usage example:
 * <pre><code>
 * SomeInstrumentedType::template visitAllFields<Visitor>();
 * </code></pre>
 * or
 * <pre><code>
 * SomeInstrumentedType obj;
 * // obj is passed to Visitor::Visitor(...) constructor.
 * obj.template visitAllFields<Visitor>(obj);
 * </code></pre>
 */
template<typename Visitor>
class GenericVisitor
{
public:
    template<typename... Fields>
    auto operator()(Fields... fields)
    {
        (self().visitField(fields), ...);
        return self().finish();
    }

protected:
    /**
     * Declare method with the same signature and (if needed) a different return type to
     * make visitAllFields return some value.
     */
    void finish() {}

private:
    Visitor& self()
    {
        return static_cast<Visitor&>(*this);
    }
};

} // namespace nx::reflect

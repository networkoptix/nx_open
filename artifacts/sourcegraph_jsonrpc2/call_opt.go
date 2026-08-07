package jsonrpc2

// CallOption is an option that can be provided to (*Conn).Call to
// configure custom behavior. See Meta.
type CallOption interface {
	apply(r *Request) error
}

type callOptionFunc func(r *Request) error

func (c callOptionFunc) apply(r *Request) error { return c(r) }

// Meta returns a call option which attaches the given meta object to
// the JSON-RPC 2.0 request (this is a Sourcegraph extension to JSON
// RPC 2.0 for carrying metadata).
func Meta(meta interface{}) CallOption {
	return callOptionFunc(func(r *Request) error {
		return r.SetMeta(meta)
	})
}

// ExtraField returns a call option which attaches the given name/value pair to
// the JSON-RPC 2.0 request. This can be used to add arbitrary extensions to
// JSON RPC 2.0.
func ExtraField(name string, value interface{}) CallOption {
	return callOptionFunc(func(r *Request) error {
		return r.SetExtraField(name, value)
	})
}

// PickID returns a call option which sets the ID on a request. Care must be
// taken to ensure there are no conflicts with any previously picked ID, nor
// with the default sequence ID.
func PickID(id ID) CallOption {
	return callOptionFunc(func(r *Request) error {
		r.ID = id
		return nil
	})
}

// ResponseCallback is invoked from the connection read loop, so it must not block. It runs
// before the call's Waiter is released. It is not invoked if the connection is closed while the
// call is still outstanding.
type ResponseCallback func(resp *Response, err error)

type onResponseOption struct{ cb ResponseCallback }

func (onResponseOption) apply(*Request) error { return nil }

// OnResponse delivers the response to cb instead of making the caller wait for it. Used with
// DispatchCall, whose Waiter can then be discarded.
func OnResponse(cb ResponseCallback) CallOption {
	return onResponseOption{cb: cb}
}

// StringID returns a call option that instructs the request ID to be set as a
// string.
func StringID() CallOption {
	return callOptionFunc(func(r *Request) error {
		r.ID.IsString = true
		return nil
	})
}

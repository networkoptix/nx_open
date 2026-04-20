package jsonrpc2

import (
	"encoding/json"
	"errors"
)

type inputMessage struct {
	request  *Request
	response *Response
}

func (m *inputMessage) parseRequest(raw map[string]json.RawMessage) error {
	m.request = &Request{}
	r := m.request
	if err := json.Unmarshal(raw["method"], &r.Method); err != nil {
		return err
	}
	if params, has := raw["params"]; has {
		r.Params = &params
		delete(raw, "params")
	}
	if meta, has := raw["meta"]; has {
		r.Params = &meta
		delete(raw, "meta")
	}
	if id, has := raw["id"]; has {
		if err := json.Unmarshal(id, &r.ID); err != nil {
			return err
		}
		delete(raw, "id")
	} else {
		r.ID = ID{}
		r.Notif = true
	}
	// The jsonrpc field should not be added to ExtraFields.
	delete(raw, "jsonrpc")
	// Clear the extra fields before populating them again.
	r.ExtraFields = nil
	for name, rawValue := range raw {
		var value interface{}
		if err := json.Unmarshal(rawValue, &value); err != nil {
			return err
		}
		r.ExtraFields = append(r.ExtraFields, RequestField{
			Name:  name,
			Value: value,
		})
	}
	return nil
}

func (m *inputMessage) parseResponse(raw map[string]json.RawMessage) error {
	m.response = &Response{}
	r := m.response
	if id, has := raw["id"]; has {
		if err := json.Unmarshal(id, &r.ID); err != nil {
			return err
		}
	}
	if result, has := raw["result"]; has {
		r.Result = &result
	}
	if rawErr, has := raw["error"]; has {
		if err := json.Unmarshal(rawErr, &r.Error); err != nil {
			return err
		}
	}
	if meta, has := raw["meta"]; has {
		r.Meta = &meta
	}
	if extensions, has := raw["extensions"]; has {
		if err := json.Unmarshal(extensions, &r.NxExtensions); err != nil {
			return err
		}
	}
	return nil
}

func (m *inputMessage) UnmarshalJSON(data []byte) error {
	rawMessage := map[string]json.RawMessage{}
	if err := json.Unmarshal(data, &rawMessage); err != nil {
		return err
	}
	if _, has := rawMessage["jsonrpc"]; !has {
		return errors.New("jsonrpc2: missing jsonrpc field")
	}
	_, hasMethod := rawMessage["method"]
	_, hasResult := rawMessage["result"]
	_, hasError := rawMessage["error"]
	isRequest := hasMethod
	isResponse := hasResult || hasError

	if (!isRequest && !isResponse) || (isRequest && isResponse) {
		return errors.New("jsonrpc2: unable to determine message type (request or response)")
	}
	if isRequest {
		return m.parseRequest(rawMessage)
	}
	return m.parseResponse(rawMessage)
}

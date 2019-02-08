package main

import (
	"bytes"
	"compress/gzip"
	"fmt"
	"io"
	"strings"
)

func bindata_read(data []byte, name string) ([]byte, error) {
	gz, err := gzip.NewReader(bytes.NewBuffer(data))
	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}

	var buf bytes.Buffer
	_, err = io.Copy(&buf, gz)
	gz.Close()

	if err != nil {
		return nil, fmt.Errorf("Read %q: %v", name, err)
	}

	return buf.Bytes(), nil
}

var _cloud_modules_xml_template = []byte("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff\x7c\x90\xb1\x4e\xc5\x30\x0c\x45\x77\xbe\xc2\x8a\x58\x69\xf6\xaa\x65\x29\x03\x0b\x88\x89\x15\xa5\x89\xa1\x91\x52\xbb\x24\xce\x14\xe5\xdf\x51\x01\xa9\xd5\x6b\xdf\x1b\x73\x23\xdf\x7b\x74\xba\x84\xdf\x19\xc9\xe2\xe3\x1d\x00\x40\x97\x50\x20\x62\x7a\x35\x33\xf6\xca\xba\x51\xad\xaf\x77\x13\x32\xf6\x6a\x12\x59\x52\xab\x75\x29\xcd\x10\x38\xbb\xa7\xb1\x79\xe6\x24\xb5\xb6\xbb\xe4\x8d\xa3\xd4\xaa\xf4\x5f\x5f\x29\x10\x0d\x7d\x21\xdc\x0f\x4c\x84\x56\x3c\xd3\x0b\x3a\x6f\x84\x23\xb4\x3d\x34\xc7\x38\xc1\x43\xad\x27\x30\xd3\x32\xef\x61\x92\x64\xfa\x65\x39\x69\xde\xb0\xce\x3e\x0f\x84\x48\xee\xca\x26\xb1\xf8\x4f\x6f\xcd\x7a\xff\x31\xb3\xcb\x01\x6f\x09\x59\x9b\x4d\xb8\x94\xf2\x9f\x6e\xb3\x9d\xde\xa4\xff\x04\x00\x00\xff\xff\xd8\x13\xde\x02\x80\x01\x00\x00")

func cloud_modules_xml_template() ([]byte, error) {
	return bindata_read(
		_cloud_modules_xml_template,
		"cloud_modules.xml.template",
	)
}

// Asset loads and returns the asset for the given name.
// It returns an error if the asset could not be found or
// could not be loaded.
func Asset(name string) ([]byte, error) {
	cannonicalName := strings.Replace(name, "\\", "/", -1)
	if f, ok := _bindata[cannonicalName]; ok {
		return f()
	}
	return nil, fmt.Errorf("Asset %s not found", name)
}

// AssetNames returns the names of the assets.
func AssetNames() []string {
	names := make([]string, 0, len(_bindata))
	for name := range _bindata {
		names = append(names, name)
	}
	return names
}

// _bindata is a table, holding each asset generator, mapped to its name.
var _bindata = map[string]func() ([]byte, error){
	"cloud_modules.xml.template": cloud_modules_xml_template,
}
// AssetDir returns the file names below a certain
// directory embedded in the file by go-bindata.
// For example if you run go-bindata on data/... and data contains the
// following hierarchy:
//     data/
//       foo.txt
//       img/
//         a.png
//         b.png
// then AssetDir("data") would return []string{"foo.txt", "img"}
// AssetDir("data/img") would return []string{"a.png", "b.png"}
// AssetDir("foo.txt") and AssetDir("notexist") would return an error
// AssetDir("") will return []string{"data"}.
func AssetDir(name string) ([]string, error) {
	node := _bintree
	if len(name) != 0 {
		cannonicalName := strings.Replace(name, "\\", "/", -1)
		pathList := strings.Split(cannonicalName, "/")
		for _, p := range pathList {
			node = node.Children[p]
			if node == nil {
				return nil, fmt.Errorf("Asset %s not found", name)
			}
		}
	}
	if node.Func != nil {
		return nil, fmt.Errorf("Asset %s not found", name)
	}
	rv := make([]string, 0, len(node.Children))
	for name := range node.Children {
		rv = append(rv, name)
	}
	return rv, nil
}

type _bintree_t struct {
	Func func() ([]byte, error)
	Children map[string]*_bintree_t
}
var _bintree = &_bintree_t{nil, map[string]*_bintree_t{
	"cloud_modules.xml.template": &_bintree_t{cloud_modules_xml_template, map[string]*_bintree_t{
	}},
}}

package main

import (
	"log"
	"os"
	"strconv"
    "text/template"

	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
//	"k8s.io/client-go/tools/clientcmd"
	"net/http"
)

func GetIntEnvVar(variable string)(int32) {
	value := os.Getenv(variable)
	intval, _ := strconv.Atoi(value)
	return int32(intval)
}

func stringInSlice(a string, list []string) bool {
    for _, b := range list {
        if b == a {
            return true
        }
    }
    return false
}

func handler(w http.ResponseWriter, r *http.Request) {
    data, err := Asset("cloud_modules.xml.template")
    if err != nil {
        panic("Template is not found")
    }

    tmpl, err := template.New("first").Parse(string(data))
	if err != nil {
		panic(err.Error())
	}

    type Endpoint struct {
        Host string
        Port int32
    }

    type Context struct {
        CloudDb Endpoint
        ConnectionMediators []Endpoint
        CloudPortal Endpoint
    }

	// creates the in-cluster config
	config, err := rest.InClusterConfig()
	// config, err := clientcmd.BuildConfigFromFlags("", "/Users/ivigasin/.nxcloud/instances/cloud-testk/kubeconfig")
	if err != nil {
		panic(err)
	}

	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		panic(err)
	}

	services, err := clientset.CoreV1().Services("").List(metav1.ListOptions{LabelSelector: "app=connection-mediator"})
	if err != nil {
		panic(err)
	}

	mediatorPort := int32(0);

	for _, v := range services.Items[0].Spec.Ports {
		if v.Protocol == "UDP" {
			mediatorPort = v.NodePort
			break
		}
	}

	if mediatorPort == 0 {
		panic("Couldn't find mediator")
	}

	var mediatorNodes []string;
	pods, err := clientset.CoreV1().Pods("default").List(metav1.ListOptions{LabelSelector: "app=connection-mediator"})
	for _, pod := range pods.Items {
		if pod.Status.Phase == "Running" {
			mediatorNodes = append(mediatorNodes, pod.Spec.NodeName)
		}
	}

	nodes, err := clientset.CoreV1().Nodes().List(metav1.ListOptions{LabelSelector: "kubernetes.io/role=node"})
	if err != nil {
		panic(err)
	}

	var mediatorEndpoints []Endpoint

	for _, node := range nodes.Items {
		if !stringInSlice(node.Name, mediatorNodes) {
			continue
		}

		for _, address := range node.Status.Addresses {
			if address.Type == "ExternalIP" {
				mediatorEndpoints = append(mediatorEndpoints, Endpoint{address.Address, mediatorPort})
			}
		}
	}

    context := Context{
		Endpoint{os.Getenv("CLOUD_DB_HOST"), GetIntEnvVar("CLOUD_DB_SPORT")},
		mediatorEndpoints,
		Endpoint{os.Getenv("CLOUD_PORTAL_HOST"), GetIntEnvVar("CLOUD_PORTAL_SPORT")},
	}

    err = tmpl.Execute(w, context)
	if err != nil {
		panic(err)
	}
}

func main() {
	http.HandleFunc("/api/cloud_modules.xml", handler)
    log.Fatal(http.ListenAndServe(":80", nil))
}

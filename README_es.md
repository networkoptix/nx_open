# Componentes de código abierto de Nx Meta Platform

<!-- hy-mt2-i18n:start -->
[English](./readme.md) | [中文](./README_zh-CN.md) | [日本語](./README_ja.md) | **Español**
<!-- hy-mt2-i18n:end -->


// Copyright 2018-present Network Optix, Inc. Licenciado bajo MPL 2.0: www.mozilla.org/MPL/2.0/

---------------------------------------------------------------------------------------------------
## Introducción

Este repositorio `nx_open` contiene los **componentes de código abierto de Network Optix Meta Platform**: el código fuente y las especificaciones que se utilizan para desarrollar todos los productos basados en Nx, incluido el Sistema de Gestión de Vídeo Nx Witness (VMS).

Actualmente, el principal componente del VMS que se puede desarrollar a partir de este repositorio es el Cliente de escritorio. Otros componentes destacados que forman parte del Cliente de escritorio, pero que también pueden ser útiles de forma independiente, incluyen la biblioteca Nx Kit (`artifacts/nx_kit/`); consulte su `readme.md` para obtener más detalles.

La mayor parte del código fuente y otros archivos están licenciados bajo los términos de la Licencia Pública de Mozilla 2.0 (a menos que se especifique lo contrario en los archivos), la cual se puede encontrar en el archivo `license_mpl2.md` dentro de la carpeta `licenses/`, en la raíz del repositorio.

ATENCIÓN: Este documento solo proporciona información breve sobre el proceso de compilación y sus requisitos previos, específicos para la rama actual. Para obtener las instrucciones más precisas sobre cómo configurar el entorno de compilación, una explicación detallada del funcionamiento interno del sistema de compilación y recomendaciones para utilizar herramientas de compilación y desarrollo, consulte el siguiente documento en la rama `master` de este repositorio: [build.md](https://github.com/networkoptix/nx_open/blob/master/build.md)

---------------------------------------------------------------------------------------------------
## Avisos sobre software libre y de código abierto

El software “Componentes de código abierto de Network Optix Meta Platform” incorpora, depende de, interactúa con, o fue desarrollado utilizando una serie de componentes de software libre y de código abierto. La lista completa de dichos componentes se puede encontrar en [REVELACIÓN DE SOFTWARE DE CÓDIGO ABIERTO](https://meta.nxvms.com/content/libraries/). Consulte los sitios web de los componentes enlazados para obtener información adicional sobre licencias, dependencias y uso, así como el código fuente de los componentes.

---------------------------------------------------------------------------------------------------
## Entorno de compilación

Plataformas y arquitecturas de destino compatibles:
- Windows 10 x64 (Microsoft Visual Studio).
- Linux Ubuntu 20.04, 22.04, 24.04 (GCC o Clang) x64, ARM 32/64 (compilación cruzada en Linux x64).
- macOS Monterey 12.6.3 (Xcode con Clang) x64, Apple M1/M2.

Requisitos previos para la compilación:  
- **Python 3.8+**: debe estar disponible en `PATH` como `python`, y también como `python3` en macOS y Ubuntu.  
- **Pip**: debe estar disponible en `PATH` como `pip` e instalado para el intérprete de Python utilizado en la compilación.  
- **CMake, Ninja, Conan**: se recomienda instalarlos mediante **`pip`** a partir del archivo `requirements.txt` de la rama `master`; puede encontrar las versiones requeridas en este archivo.  
- **Linux**: Instale las **dependencias de compilación y ejecución** mediante CMake especificando el parámetro de línea de comandos `cmake` `-DinstallSystemRequirements=ON` en la fase de generación (puede solicitarse la contraseña de `sudo`).  
  - NOTA: El compilador se descarga como un artefacto de Conan durante la fase de generación de CMake; los compiladores instalados en el sistema Linux (si los hay) no se utilizan.  
- **Windows**: Microsoft Visual Studio 2022, [Community Edition](https://visualstudio.microsoft.com/downloads/); seleccione los componentes:  
  - “The Workload” -> “Desktop development with C++”  
  - “Individual components” -> “C++ CMake tools for Windows”  
- **macOS**: **Xcode Command Line Tools 14.2+**; también instale las siguientes **dependencias de compilación**:  
  - Para Apple M1/M2, instale Rosetta 2:  
    ```
    /usr/sbin/softwareupdate --install-rosetta --agree-to-license
    ```

---------------------------------------------------------------------------------------------------
## Compilación del cliente de escritorio de VMS

El cliente utiliza en su interfaz gráfica una colección de textos y gráficos denominada Paquete de Personalización; este define la imagen de marca del VMS. El Paquete de Personalización se entrega en formato archivo zip. Se utiliza uno predeterminado proveniente de Conan: el cliente tendrá como marca “Nx Meta” e mostrará marcadores de posición para elementos como el nombre de la empresa, la página web y el texto del Acuerdo de Licencia de Usuario Final. Si desea definir estos elementos, cree una entidad “Cliente Personalizado” en el Portal de Desarrolladores de Nx Meta y descargue el archivo zip del Paquete de Personalización generado en https://meta.nxvms.com/developers/custom-clients/. Allí también podrán encontrarse Paquetes de Personalización con otras marcas distintas a Nx Meta.

Todos los comandos necesarios para realizar las etapas de configuración y compilación con CMake están escritos en los scripts `build.sh` (para Linux y macOS) y `build.bat` (para Windows), ubicados en la raíz del repositorio. Por favor, considere estos scripts como una guía de inicio rápido, estudie su código fuente y no dude en utilizar su propio flujo de trabajo de desarrollo en C++.

Estos scripts crean o utilizan el directorio de compilación como un elemento hermano del directorio raíz del repositorio, al cual se le agrega el sufijo “-build”. Suponemos que el directorio raíz del repositorio es “nx_open/”, por lo que el directorio de compilación será “nx_open-build/”.

ATENCIÓN: Si la generación falla por cualquier motivo, elimine manualmente el archivo `CMakeCache.txt` antes de intentar ejecutar el script de compilación nuevamente.

A continuación se presentan los ejemplos de uso; en ellos, `<build>` corresponde a `./build.sh` para Linux y macOS, y a `build.bat` para Windows.

- Para crear una compilación de depuración limpia, elimine el directorio de compilación (si existe) y ejecute el comando:
    ```
    <build>
    ```
    Los ejecutables generados se colocarán en `nx_open-build/bin/`.

- Para crear una compilación de versión estable limpia que incluya el paquete de distribución y el archivo de pruebas unitarias, elimine el directorio de compilación (si existe) y ejecute el comando:
    ```
    <build> -DdeveloperBuild=OFF
    ```
    Los paquetes de distribución generados y el archivo de pruebas unitarias se colocarán en `nx_open-build/distrib/`. Para ejecutar las pruebas unitarias, descomprima el archivo de pruebas unitarias y ejecute todos los ejecutables que contiene, ya sea uno por uno o en paralelo.

- Para utilizar el Paquete de Personalización obtenido en lugar del predeterminado proporcionado por Conan (con la marca Nx-Meta y marcadores de posición), agregue los siguientes argumentos al script `<build>`:
    ```
    -DcustomizationPackageFile=<customization.zip>
    ```
    NOTA: El valor del campo `"id":` en `description.json` dentro del archivo zip especificado debe coincidir con el del servidor para poder conectarse a él.

- Para realizar una compilación incremental tras realizar algunos cambios, ejecute el script `<build>` sin argumentos.
    - Tenga en cuenta que no es necesario llamar explícitamente a la etapa de Generación después de agregar o eliminar archivos de fuente o modificar los archivos del sistema de compilación, ya que `ninja_tool.py` gestiona adecuadamente tales casos: la etapa de Generación se ejecutará automáticamente cuando sea necesario.

Para la **compilación cruzada** en Linux o macOS, establezca la variable CMake `<targetDevice>`: agregue el argumento `-DtargetDevice=<value>`, donde <value> es uno de los siguientes:  
- `linux_x64`  
- `linux_arm64`  
- `linux_arm32`  
- `macos_x64`  
- `macos_arm64`

También se admite la compilación y depuración en el IDE Visual Studio: ejecute la fase de Generación desde la línea de comandos; esto creará los archivos `CMakeSettings.json` y `launch.vs.json`, y a continuación se abrirá el proyecto.

Se recomienda establecer la variable de entorno `NX_CONAN_DOWNLOAD_CACHE` con la ruta completa de un directorio que se utilizará para evitar volver a descargar todos los archivos resultantes desde Internet en cada compilación limpia; por ejemplo, cree el directorio `conan_cache/` junto a la raíz del repositorio y los directorios de compilación.

---------------------------------------------------------------------------------------------------
## Firma de archivos ejecutables

- **Windows**:

    Existe una opción para firmar los ejecutables generados (incluido el propio archivo de distribución) con el certificado del editor del software. Para ello, se necesita un archivo de certificado válido en formato PKCS#12.

    La firma se realiza mediante el script `signtool.py`, que es un envoltorio alrededor del comando nativo de Windows `signtool.exe`. Para habilitar la firma, es necesario seguir estos pasos previos:  
- Guardar el archivo del certificado del editor en alguna parte del sistema de archivos.  
- Crear el archivo de configuración (de preferencia fuera del árbol de fuentes). Este archivo debe contener los siguientes campos:  
  - `file`: la ruta del archivo del certificado del editor. Debe tratarse de una ruta absoluta o de una ruta relativa al directorio donde se encuentra el archivo de configuración.  
  - `password`: la contraseña que protege el archivo del certificado del editor.  
  - `timestamp_servers` (opcional): una lista de las URL de los servidores de marca de tiempo de confianza. Si este campo está presente en el archivo de configuración, el archivo firmado recibirá una marca de tiempo utilizando uno de los servidores indicados. Si este campo falta, el archivo firmado no tendrá marca de tiempo.

        Un ejemplo de archivo de configuración se encuentra en
        `build_utils/signtool/config/config.yaml`.
    - Agregue un argumento de CMake `-DsigntoolConfig=<ruta_del_archivo_de_configuración>` en la etapa de generación. Si este argumento falta, no se realizará la firma.

    También puede firmar cualquier archivo manualmente llamando directamente a `signtool.py`:
    `python build_utils/signtool/signtool.py --config <configuration_file> --file <unsigned_file> --output <signed_file>`

    Para probar el procedimiento de firma, puede utilizar un certificado autofirmado. Para generar dicho certificado, puede usar el archivo `build_utils/signtool/genkey/genkey_signtool.bat`. Al ejecutarse, crea el archivo `certificate.p12` y varios archivos auxiliares de tipo `*.pem` en el mismo directorio donde se ejecuta. Recomendamos mover estos archivos fuera del directorio de fuentes para mantener el concepto de compilación fuera de las fuentes.

- **Linux**:

    No se requiere firmar; no se proporcionan herramientas ni instrucciones.

- **macOS**:

    Se está desarrollando una herramienta de firma adecuada para uso independiente y probablemente estará disponible en el futuro. Por ahora, puede utilizar el procedimiento de firma habitual que emplea para sus otros proyectos en macOS.

---------------------------------------------------------------------------------------------------
## Ejecutar el VMS Desktop Client

Se puede ejecutar el VMS Desktop Client directamente desde el directorio de compilación, sin necesidad de instalar un paquete de distribución.

Tras una compilación exitosa, el ejecutable del Cliente de escritorio se encuentra en `nx_open-build/bin/`; su nombre puede depender del Paquete de personalización.

Para **Linux** y **macOS**, basta con ejecutar el ejecutable del Cliente de escritorio.

Para **Windows**, antes de ejecutar el ejecutable del Cliente de escritorio o cualquier otro ejecutable generado, ejecute en la consola el siguiente script (generado por Conan durante la compilación), el cual establece correctamente PATH y algunas otras variables de entorno:
```
nx_open-build/activate_run.bat
```
Para restaurar los valores originales de las variables, incluido PATH, puede ejecutar el siguiente script:
```
nx_open-build/deactivate_run.bat
```

### Versiones compatibles del servidor VMS

El cliente de escritorio compilado a partir del repositorio de código abierto solo puede conectarse a un servidor VMS compatible. Dado que los códigos fuente del servidor VMS no están disponibles públicamente, dicho servidor solo se puede obtener de alguna versión pública de VMS, incluidas las versiones oficiales de VMS y las versiones de previsualización regulares denominadas Nx Meta VMS.

Para cada versión pública de VMS, la compatibilidad está garantizada únicamente para el Cliente construido a partir del mismo commit que el Servidor. Ese commit específico puede identificarse en el repositorio mediante su etiqueta git. Las etiquetas de las versiones públicas tienen el formato `vms/4.2/12345_release_all` o `vms/5.0/34567_beta_meta_R2`.

Los clientes desarrollados a partir de commits posteriores en la misma rama pueden mantener la compatibilidad con el servidor publicado durante un tiempo, pero en algún momento podrían perderla debido a algunos cambios introducidos de forma simultánea en las partes del código fuente correspondientes al cliente y al servidor. Por lo tanto, se recomienda basar las ramas de modificación del cliente en commits etiquetados que correspondan a las versiones públicas, incluidas las versiones de Nx Meta VMS, y realizar un rebase tan pronto como esté disponible la próxima versión pública de dicha rama.

ATENCIÓN: Además de contar con código compatible, para que puedan funcionar juntos, el Cliente y el Servidor deben utilizar Paquetes de Personalización con el mismo valor de `<customization_id>`.

Durante la fase de generación, el sistema de compilación intenta determinar la versión del Servidor compatible revisando las etiquetas de git. Busca el primer commit en común entre la rama actual y una de las ramas “protegidas” (que corresponden a versiones estables de VMS), y verifica si cuenta con una etiqueta “release” del formato “vms/#.#/#####_...”. Si no se encuentra dicha etiqueta, el número de compilación se establece en 0 y se genera una advertencia; de lo contrario, el número de compilación se extrae de la etiqueta. Para omitir este algoritmo, especifique “-DbuildNumber=<custom_build_number>” en cmake; para volver a utilizarlo, realice una compilación limpia o especifique “-DbuildNumber=AUTO”.

### Actualizaciones automáticas de VMS

El producto VMS incluye un soporte integral para actualizaciones automáticas, pero esta función está desactivada en el Cliente de escritorio de código abierto, ya que simplemente sobrescribiría dicho cliente con la nueva versión generada por Nx. Cabe señalar que el administrador de VMS aún puede forzar dicha actualización automática, asumiendo las consecuencias mencionadas.

Técnicamente es posible especificar un servidor de actualizaciones personalizado en la configuración del servidor VMS, desplegar dicho servidor y preparar los paquetes de actualización así como la metainformación según los estándares de VMS, de modo que las actualizaciones automáticas funcionen con una versión de VMS personalizada desarrollada a partir de código abierto. En el futuro, es probable que se proporcionen instrucciones y/o herramientas al respecto.

# Essential Code Classes {#layout_classes}

@ref QnLayoutFileStorageResource is the main class for writing and reading Nov Files and Executable Layouts. Plain streams inside Nov file are accessed via @ref QnLayoutPlainStream and encrypted streams are acccessed via @ref QnLayoutCryptoStream. `QnLayoutCryptoStream` is based on @ref nx::utils::CryptedFileStream, which is a general purpose file crypto-container, layout-unaware and suitable for other purposes.

QnLayoutFileStorageResource always keep track of all its opened streams as well as all opened streams are attached to the parent QnLayoutFileStorageResource. 

![](images/layout_storage.svg)



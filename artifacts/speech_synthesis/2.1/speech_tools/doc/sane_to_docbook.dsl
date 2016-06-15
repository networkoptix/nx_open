<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
]>

(declare-flow-object-class element
  "UNREGISTERED::James Clark//Flow Object Class::element")
(declare-flow-object-class empty-element
  "UNREGISTERED::James Clark//Flow Object Class::empty-element")
(declare-flow-object-class document-type
  "UNREGISTERED::James Clark//Flow Object Class::document-type")
(declare-flow-object-class processing-instruction
  "UNREGISTERED::James Clark//Flow Object Class::processing-instruction")
(declare-flow-object-class entity
  "UNREGISTERED::James Clark//Flow Object Class::entity")
(declare-flow-object-class entity-ref
  "UNREGISTERED::James Clark//Flow Object Class::entity-ref")
(declare-flow-object-class formatting-instruction
  "UNREGISTERED::James Clark//Flow Object Class::formatting-instruction")
(declare-characteristic preserve-sdata?
  "UNREGISTERED::James Clark//Characteristic::preserve-sdata?"
  #f)

(define sect-depth 1)
(define unique-name (number->string (time)))

(define (copy-attributes #!optional (nd (current-node)))
  (let loop ((atts (named-node-list-names (attributes nd))))
    (if (null? atts)
        '()
        (let* ((name (car atts))
               (value (attribute-string name nd)))
          (if value
              (cons (list name value)
                    (loop (cdr atts)))
              (loop (cdr atts)))))))

(default   
  (empty-sosofo)
  )

(element sane 
	 (process-children)
	 )

(define (sect-at depth)
  (if (> depth 0)
      (string-append "sect" (number->string depth))
      "chapter"
      )
  )

(define (handle-container-item type depth)
  (let* ((id (attribute-string "id"))
	 )
    (make element gi: (sect-at depth)
	  attributes: (list (list "id" id) (list "role" type))
	  (make sequence
		(with-mode build-title (process-node-list (current-node)))
		(if (equal? depth sect-depth)
		    (make element gi: "toc"
			  attributes: '(("depth" "5"))
			  (empty-sosofo)
			  )
		    (empty-sosofo)
		    )
		(with-mode text (process-matching-children "cxxdoc"))
		(process-matching-children "cxxPublic")
		(process-matching-children "cxxProtected")
		(process-matching-children "cxxPrivate")
		(process-matching-children "cxxItems")
		)
	  )
    )
  )

(define (handle-item type depth)
  (let* ((id (attribute-string "id"))
	 )
    (make element gi: "simpleSect"
	  attributes: (list (list "id" id) (list "role" type))
	  (make sequence
		(with-mode build-title (process-node-list (current-node)))
		(with-mode text (process-matching-children "cxxdoc"))
		(with-mode parameters (process-matching-children "parameters"))
		(process-matching-children "cxxPublic")
		(process-matching-children "cxxProtected")
		(process-matching-children "cxxPrivate")
		(process-matching-children "cxxItems")
		)
	  )
    )
  )

(define ($xref$)
	 (let* ((id (attribute-string "linkend")))
	   (make element gi: "xref"
			 attributes: (list (list "linkend" id))
			 (empty-sosofo)
			 )
	   )
	 )

(define (find-depth nd)
  (let* ((p (parent nd)))
    (if (null? p)
	0
	(case (gi p)
	  (("CXXCLASS" "CXXINTERFACE" "CXXUNION" "CXXSTRUCT" "CXXENTRY") 
	   (+ 1 (find-depth p)))
	  (("SANE") 
	   0)
	  (else 
	   (find-depth p))
	  )
	)
    )
  )

(define (sgml-root-element)
  ;; Returns the node that is the root element of the current document
  (let loop ((root (current-node)))
    (if (node-list-empty? (parent root))
        root
        (loop (parent root)))))

(define (node-depth)
  (+ sect-depth (find-depth (current-node)))
  )

(define (unique-filename base ext)
  (string-append base "_" unique-name "_" (number->string (element-number)) "." ext)
  )
    

(element docppRef
	 ($xref$)
	 )

(element docppLink
	 (make element gi: "link"
	      attributes: (copy-attributes)
	      )
	)

(element cxxClass
	 (handle-container-item "class" (node-depth))
	 )

(element cxxUnion
	 (handle-container-item "union" (node-depth))
	 )

(element cxxInterface
	 (handle-container-item "interface" (node-depth))
	 )

(element cxxStruct
	 (handle-container-item "struct" (node-depth))
	 )

(element cxxEntry
	 (handle-container-item "entry" (node-depth))
	 )

(element cxxMethod
	 (handle-item "method" (node-depth))
	 )

(element cxxVariable
	 (handle-item "variable" (node-depth))
	 )

(element cxxFunction
	 (handle-item "function" (node-depth))
	 )

(element cxxMacro
	 (handle-item "macro" (node-depth))
	 )

(element cxxPublic
	 (process-children)
	 )
(element cxxProtected
	 (process-children)
	 )
(element cxxPrivate
	 (process-children)
	 )
(element cxxItems
	 (process-children)
	 )

(define ($cpp-title$ before after)
  (make sequence
	(make element gi: "title"
	      (literal before)
	      (with-mode code (process-matching-children "name"))
	      (literal after)
	      )
	(make element gi: "synopsis"
	      (make sequence
		    (make element gi: "synopsis"
			  attributes: '(("role" "type"))
			  (with-mode code (process-matching-children "type"))
			  )
		    (make element gi: "synopsis"
			  attributes: '(("role" "name"))
			  (with-mode code (process-matching-children "name"))
			  )
		    (make element gi: "synopsis"
			  attributes: '(("role" "args"))
			  (with-mode code (process-matching-children "args"))
			  )
		    )
	      )
	)
  )
(define ($title$)
  (make element gi: "title"
	(with-mode text (process-matching-children "name"))
	)
  )


(mode build-title

      (default (empty-sosofo))
      
      
      (element cxxClass ($cpp-title$ "class " "{}"))
      (element cxxInterface ($cpp-title$ "interface " "{}"))
      (element cxxUnion ($cpp-title$ "union " "{}"))
      (element cxxStruct ($cpp-title$ "struct " "{}"))
      (element cxxMethod ($cpp-title$ "" "()"))
      (element cxxFunction ($cpp-title$ "" "()"))
      (element cxxVariable ($cpp-title$ "" ";"))
      (element cxxMacro ($cpp-title$ "#define " "()"))
      (element cxxEntry ($title$))
      )

(mode code

      (default
	(make element
	      attributes: (copy-attributes))
	)

      (element docppRef
	       ($xref$)
	       )

      (element docppLink
	       (make element gi: "link"
		     attributes: (copy-attributes)
		     )
	       )

      (element args
	       (process-children)
	       )

      (element type
	       (process-children)
	       )

      (element name
	       (if (string=? (data (current-node)) "()")
		   (literal "operator")
		   (process-children)
		   )
	       )
      )


(mode text
      
      (default 
	(make element
	      attributes: (copy-attributes))
	)

      (element math
	 (let* ((file (unique-filename "docppmath" "gif")))
	   (make element gi: "graphic"
		 attributes: (list '("format" "tex")
				   (list "fileref" file))
		 )
	      )
	)

      (element imath
	 (let* ((file (unique-filename "docppimath" "gif")))
	   (make element gi: "inlinegraphic"
		 attributes: (list '("format" "tex")
				   (list "fileref" file))
		 )
	      )
	)

      (element docppRef
	       ($xref$)
	       )

      (element docppLink
	       (make element gi: "link"
		     attributes: (copy-attributes)
		     )
	       )

      (element cxxdoc
	       (process-children)
	       )

      (element expr
	       (make element gi: "literal"
		     attributes: (list (list "class" "expression"))
		     (process-children)
		     )
	       )
      (element method
	       (make element gi: "function"
		     attributes: (list (list "class" "method"))
		     (process-children)
		     )
	       )

      (element (parameter name)
	       (empty-sosofo)
	       )

      (element name
	       (process-children)
	       )
      
      )

(mode parameters

      (element parameters
	       (make element gi: "variableList"
		     (make sequence
			   (make element gi: "title"
				 (literal "Parameters")
				 )
			   (process-children)
			   )
		     )
	       )

      (element parameter
	       (make element gi: "varListEntry"
		     (make sequence
			   (make element gi: "term"
				 (with-mode code (process-matching-children "name"))
				 )
			   (make element gi: "listitem"
				 (make element gi: "para"
				       (with-mode text (process-children))
				       )
				 )
			   )
		     )
	       )

      )

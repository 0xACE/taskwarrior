" Vim syntax file
" Language:	task data
" Maintainer:	John Florian <jflorian@doubledog.org>


" For version 5.x: Clear all syntax items.
" For version 6.x: Quit when a syntax file was already loaded.
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

" Key Names for values.
syn keyword taskdataKey		description due end entry imask mask parent
syn keyword taskdataKey	 	priority project recur start status tags uuid
syn match taskdataKey		"annotation_\d\+"

" Values associated with key names.
"
" Strings
syn region taskdataString	matchgroup=Normal start=+"+ end=+"+
		    \ 	contains=taskdataEncoded,taskdataUUID,@Spell
"
" Special Embedded Characters (e.g., "&comma;")
syn match taskdataEncoded	"&\a\+;" contained
" UUIDs
syn match taskdataUUID 	"\x\{8}-\(\x\{4}-\)\{3}\x\{12}" contained


" The default methods for highlighting.  Can be overridden later.
hi def link taskdataEncoded	Function
hi def link taskdataKey		Statement
hi def link taskdataString 	String
hi def link taskdataUUID 		Special

let b:current_syntax = "taskdata"

" vim:noexpandtab

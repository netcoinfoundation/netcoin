HOW TO MAKE CUSTOM WALLET:

Add 2 .jpg images in the custom folder.
For other formats change the .jpg extension in Overview and Dialogs (just after [VARS]) in the styles.qss.

For overview page  name image:   custom.
For all other pages name image:    custom1.

Find a color hex(search google)for the text and     change in styles.qss:     
-- $text-color = #hex
-- $accent-color = #hex
-- $main-color-xl = #hex

Those 3 vars are usually enough to get a nice looking costum wallet but you can always play with the other vars in the styles.qss.

To change the font size go in styles.qss and change font-size px in:
-- QDialog
-- QGroupbox
-- QWidget
-- QLabel
-- QToolBar QToolButton

Reload the theme to see the result after a change.

Happy customizing :)
 

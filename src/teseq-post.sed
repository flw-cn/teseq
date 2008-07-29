#!/bin/sed -f

# Copying and distribution of this file, with or without modification,
# are permitted in any medium without royalty provided the copyright
# notice and this notice are preserved.

# Text: cyan, reverse-video content
/^-\{0,1\}|/ {
	s/^-\{0,1\}|/&[36;7m/ ; s/|.\{0,1\}$/[m&/
}

# Short controls: red
s/^\..*$/[31m&[m/

# Escape sequences: yellow
s/^:.*$/[33m&[m/

# Labels: magenta
s/^&.*$/[35m&[m/

# Descriptions: green
s/^".*$/[32m&[m/

# Delay lines: blue
s/^@.*$/[34m&[m/

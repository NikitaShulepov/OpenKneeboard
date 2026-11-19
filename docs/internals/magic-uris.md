---
parent: Internals
---

# Magic URIs

VDXROVR recognizes several magic URIs in PDF documents; these were added to support the 'Quick Start' PDF, but can be
used in hyperlinks in any PDF document.

Currently the following are supported:

* `vdxrovr:///Settings/Games`: Open Settings -> Games
* `vdxrovr:///Settings/Input`: Open Settings -> Input
* `vdxrovr:///Settings/Tabs`: Open Settings -> Tabs

Others can be added to `MainWindow::LaunchVDXROVRURI()` if needed.

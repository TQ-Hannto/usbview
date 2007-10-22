/*************************************************************************
** usbtree.c for USBView - a USB device viewer
** Copyright (c) 1999 by Greg Kroah-Hartman, greg@kroah.com
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** (See the included file COPYING)
*************************************************************************/


#ifdef HAVE_CONFIG_H
	#include <config.h>
#endif

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"
#include "usbtree.h"
#include "showmessage.h"
#include "usbparse.h"

#define MAX_LINE_SIZE	1000


static void Init (void)
{
	/* blow away the tree if there is one */
	if (rootDevice != NULL) {
		gtk_ctree_remove_node (GTK_CTREE(treeUSB), GTK_CTREE_NODE(rootDevice->leaf));
	}

	/* clean out the text box */
	gtk_editable_delete_text (GTK_EDITABLE(textDescription), 0, -1);

	return;
}


static void PopulateListBox (int deviceId)
{
	Device  *device;
	gint    position = 0;
	char    *string;
	char    *tempString;
	int     configNum;
	int     interfaceNum;
	int     endpointNum;
	int     deviceNumber = (deviceId >> 8);
	int     busNumber = (deviceId & 0x00ff);

	device = usb_find_device (deviceNumber, busNumber);
	if (device == NULL) {
		printf ("Can't seem to find device info to display\n");
		return;
	}

	/* clear the textbox */
	gtk_editable_delete_text (GTK_EDITABLE(textDescription), 0, -1);

	/* freeze the display */
	/* this keeps the annoying scroll from happening */
	gtk_text_freeze (GTK_TEXT (textDescription));

	string = (char *)g_malloc (1000);

	/* add the name to the textbox if we have one*/
	if (device->name != NULL) {
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), device->name, strlen(device->name), &position);
	}

	/* add the manufacturer if we have one */
	if (device->manufacturer != NULL) {
		sprintf (string, "\nManufacturer: %s", device->manufacturer);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* add the serial number if we have one */
	if (device->serialNumber != NULL) {
		sprintf (string, "\nSerial Number: %s", device->serialNumber);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* add speed */
	switch (device->speed) {
		case 1 :        tempString = "1.5Mb/s (low)";   break;
		case 12 :       tempString = "12Mb/s (full)";   break;
		case 480 :      tempString = "480Mb/s (high)";  break;		/* planning ahead... */
		default :       tempString = "unknown";         break;
	}
	sprintf (string, "\nSpeed: %s", tempString);
	gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

	/* add ports if available */
	if (device->maxChildren) {
		sprintf (string, "\nNumber of Ports: %i", device->maxChildren);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* add the bandwidth info if available */
	if (device->bandwidth != NULL) {
		sprintf (string, "\nBandwidth allocated: %i / %i (%i%%)", device->bandwidth->allocated, device->bandwidth->total, device->bandwidth->percent);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

		sprintf (string, "\nTotal number of interrupt requests: %i", device->bandwidth->numInterruptRequests);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

		sprintf (string, "\nTotal number of isochronous requests: %i", device->bandwidth->numIsocRequests);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* add the USB version, device class, subclass, protocol, max packet size, and the number of configurations (if it is there) */
	if (device->version) {
		sprintf (string, "\nUSB Version: %s\nDevice Class: %s\nDevice Subclass: %s\nDevice Protocol: %s\n"
			 "Maximum Default Endpoint Size: %i\nNumber of Configurations: %i",
			 device->version, device->class, device->subClass, device->protocol,
			 device->maxPacketSize, device->numConfigs);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* add the vendor id, product id, and revision number (if it is there) */
	if (device->vendorId) {
		sprintf (string, "\nVendor Id: %.4x\nProduct Id: %.4x\nRevision Number: %s",
			 device->vendorId, device->productId, device->revisionNumber);
		gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
	}

	/* display all the info for the configs */
	for (configNum = 0; configNum < MAX_CONFIGS; ++configNum) {
		if (device->config[configNum]) {
			DeviceConfig    *config = device->config[configNum];

			/* show this config */
			sprintf (string, "\n\nConfig Number: %i\n\tNumber of Interfaces: %i\n\t"
				 "Attributes: %.2x\n\tMaxPower Needed: %s",
				 config->configNumber, config->numInterfaces, 
				 config->attributes, config->maxPower);
			gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

			/* show all of the interfaces for this config */
			for (interfaceNum = 0; interfaceNum < MAX_INTERFACES; ++interfaceNum) {
				if (config->interface[interfaceNum]) {
					DeviceInterface *interface = config->interface[interfaceNum];

					sprintf (string, "\n\n\tInterface Number: %i", interface->interfaceNumber);
					gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

					if (interface->name != NULL) {
						sprintf (string, "\n\t\tName: %s", interface->name);
						gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
					}

					sprintf (string, "\n\t\tAlternate Number: %i\n\t\tClass: %s\n\t\t"
						 "Sub Class: %i\n\t\tProtocol: %i\n\t\tNumber of Endpoints: %i",
						 interface->alternateNumber, interface->class, 
						 interface->subClass, interface->protocol, interface->numEndpoints);
					gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);

					/* show all of the endpoints for this interface */
					for (endpointNum = 0; endpointNum < MAX_ENDPOINTS; ++endpointNum) {
						if (interface->endpoint[endpointNum]) {
							DeviceEndpoint  *endpoint = interface->endpoint[endpointNum];

							sprintf (string, "\n\n\t\t\tEndpoint Address: %.2x\n\t\t\t"
								 "Direction: %s\n\t\t\tAttribute: %i\n\t\t\t"
								 "Type: %s\n\t\t\tMax Packet Size: %i\n\t\t\tInterval: %s",
								 endpoint->address, 
								 endpoint->in ? "in" : "out", endpoint->attribute,
								 endpoint->type, endpoint->maxPacketSize, endpoint->interval);
							gtk_editable_insert_text (GTK_EDITABLE(textDescription), string, strlen(string), &position);
						}
					}
				}
			}
		}
	}

	/* throw the cursor back to the top so the user sees the top first */
	gtk_editable_set_position (GTK_EDITABLE(textDescription), 0);

	/* thaw the display */
	gtk_text_thaw (GTK_TEXT (textDescription));

	/* clean up our string */
	g_free (string);

	return;
}



//void SelectItem (GtkWidget *widget, gpointer data)
void SelectItem (GtkWidget *widget, GtkCTreeNode *node, gint column, gpointer userData)
{
	int     data;
	data = (int) gtk_ctree_node_get_row_data (GTK_CTREE (widget), node);

	PopulateListBox ((int)data);

	return;
}


static void DisplayDevice (Device *parent, Device *device)
{
	int     i;
	gchar   *text[1];
	//GdkPixmap	*pixmap;
	//GdkBitmap	*mask;

	if (device == NULL)
		return;

	//pixmap = gdk_pixmap_create_from_xpm_d (NULL, &mask, NULL, mouse_xpm);

	text[0] = device->name;
	device->leaf = gtk_ctree_insert_node (GTK_CTREE(treeUSB), parent->leaf, NULL, text, 1, NULL, NULL, NULL, NULL, FALSE, FALSE);
	gtk_ctree_node_set_row_data (GTK_CTREE(treeUSB), device->leaf, (gpointer)((device->deviceNumber<<8) | (device->busNumber)));


#if 0	/* uncomment this when we want to show devices as not having a driver bound to it */
	/* it didn't really look good for now. */
	int             i;
	int             configNum;
	int             interfaceNum;
	char            hasChildren;
	gboolean        driverAttached = TRUE;

	if (device == NULL)
		return;

	/* determine if this device has drivers attached to all interfaces */
	for (configNum = 0; configNum < MAX_CONFIGS; ++configNum) {
		if (device->config[configNum]) {
			DeviceConfig    *config = device->config[configNum];
			for (interfaceNum = 0; interfaceNum < MAX_INTERFACES; ++interfaceNum) {
				if (config->interface[interfaceNum]) {
					DeviceInterface *interface = config->interface[interfaceNum];
					if (interface->driverAttached == FALSE) {
						driverAttached = FALSE;
						break;
					}
				}
			}
		}
	}

	/* change the color of this leaf if there are no drivers attached to it */
	if (driverAttached == FALSE) {
		GdkColor        red = {0, 0xffff, 0x0000, 0x0000};
		GtkStyle        *defaultStyle;
		GtkStyle        *style;

		/* get the current style */
		defaultStyle = gtk_widget_get_default_style();
		style = gtk_style_copy (defaultStyle);
		for (i = 0; i < 5; ++i) {
			style->fg[i] = red;
			style->bg[i] = red;
			style->text[i] = red;
			style->base[i] = red;
			style->light[i] = red;
			style->dark[i] = red;
			style->mid[i] = red;
		}
		gtk_widget_set_style (device->leaf, style);
	}

	gtk_widget_show (device->leaf);

	/* hook up our callback function to this node */
	gtk_signal_connect (GTK_OBJECT (device->leaf), "select", GTK_SIGNAL_FUNC (SelectItem),
			    (gpointer)((device->deviceNumber<<8) | (device->busNumber)));

	/* if we have children, then make a subtree */
	hasChildren = 0;
	for (i = 0; i < MAX_CHILDREN; ++i) {
		if (device->child[i]) {
			hasChildren = 1;
			break;
		}
	}
	if (hasChildren) {
		device->tree = gtk_tree_new();
		gtk_tree_item_set_subtree (GTK_TREE_ITEM(device->leaf), device->tree);
		gtk_tree_item_expand (GTK_TREE_ITEM(device->leaf));	/* make the tree expanded to start with */
	}
#endif	


	/* create all of the children's leafs */
	for (i = 0; i < MAX_CHILDREN; ++i) {
		DisplayDevice (device, device->child[i]);
	}

	return;
}



gchar devicesFile[1000];

const char *verifyMessage =     "Verify that you have USB compiled into your kernel,\n"
				"have the USB core modules loaded, and have the\n"
				"usbdevfs filesystem mounted.";

void LoadUSBTree (void)
{
	static gboolean signal_connected = FALSE;
	FILE            *usbFile;
	char            *dataLine;
	int             finished;
	int             i;


	usbFile = fopen (devicesFile, "r");
	if (usbFile == NULL) {
		gchar *tempString = g_malloc0(strlen (verifyMessage) + strlen (devicesFile) + 50);
		sprintf (tempString, "Can not open %s\n%s", devicesFile, verifyMessage);
		ShowMessage ("USBView Error", tempString);
		g_free (tempString);
		return;
	}
	finished = 0;

	Init();

	usb_initialize_list ();

	dataLine = (char *)g_malloc (MAX_LINE_SIZE);
	while (!finished) {
		/* read the line in from the file */
		fgets (dataLine, MAX_LINE_SIZE, usbFile);

		if (dataLine[strlen(dataLine)-1] == '\n')
			usb_parse_line (dataLine);

		if (feof (usbFile))
			finished = 1;
	}

	g_free (dataLine);

	usb_name_devices ();

	/* set up our tree */
	gtk_ctree_set_line_style (GTK_CTREE(treeUSB), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style (GTK_CTREE(treeUSB), GTK_CTREE_EXPANDER_SQUARE);
	gtk_ctree_set_indent (GTK_CTREE(treeUSB),10);
	gtk_clist_column_titles_passive (GTK_CLIST(treeUSB));

	/* build our tree */
	for (i = 0; i < rootDevice->maxChildren; ++i) {
		DisplayDevice (rootDevice, rootDevice->child[i]);
	}

	gtk_widget_show (treeUSB);

	gtk_ctree_expand_recursive (GTK_CTREE(treeUSB), NULL);

	/* hook up our callback function to this tree if we haven't yet */
	if (!signal_connected) {
		gtk_signal_connect (GTK_OBJECT (treeUSB), "tree-select-row", GTK_SIGNAL_FUNC (SelectItem), NULL);
		signal_connected = TRUE;
	}

	return;
}



void initialize_stuff (void)
{
	strcpy (devicesFile, "/proc/bus/usb/devices");
//	strcpy (devicesFile, "/home/greg/uhci.proc"); /* for testing... */

	return;
}


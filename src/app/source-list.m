//
// This file is part of Rip Service.
//
// Copyright (C) 2012 Chris Pickel <sfiera@sfzmail.com>
//
// Rip Service is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at
// your option) any later version.
//
// Rip Service is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Rip Service; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "source-list.h"

static NSString* DISCS = @"DISCS";

@implementation RSSourceList

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item {
    if (item == nil) {
        return 1;
    } else if (item == DISCS) {
        return 1;
    } else {
        return 0;
    }
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(id)item {
    if (item == nil) {
        return DISCS;
    } else {
        return @"Audio CD";
    }
}

- (NSView*)outlineView:(NSOutlineView*)outlineView
           viewForTableColumn:(NSTableColumn*)tableColumn item:(id)item {
    if (item == DISCS) {
        NSTextField* field = [outlineView makeViewWithIdentifier:@"HeaderCell" owner:self];
        field.stringValue = item;
        return field;
    } else {
        NSTableCellView* view = [outlineView makeViewWithIdentifier:@"MainCell" owner:self];
        view.textField.stringValue = @"Audio CD";
        return view;
    }
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isItemExpandable:(id)item {
    return [outlineView parentForItem:item] == nil;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView shouldShowOutlineCellForItem:(id)item {
    return NO;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView isGroupItem:(id)item {
    return [outlineView parentForItem:item] == nil;
}

- (BOOL)outlineView:(NSOutlineView*)outlineView shouldSelectItem:(id)item {
    return [outlineView parentForItem:item] != nil;
}

@end

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

#include <dispatch/dispatch.h>
#include <rsvc/disc.h>

static NSString* kDiscs = @"DISCS";

static NSString* kDiscName = @"kDiscName";
static NSString* kDiscType = @"kDiscType";
static NSString* kDiscTypeNames[] = {
    @"CD",
    @"DVD",
    @"Blu-Ray Disc",
};

@implementation RSSourceList

- (void)awakeFromNib {
    discs = [[NSMutableDictionary alloc] init];
    rsvc_disc_watch_callbacks_t callbacks = {
        .appeared = ^(rsvc_disc_type_t type, const char* name){
            NSString* ns_name = [[NSString alloc] initWithUTF8String:name];
            dispatch_async(dispatch_get_main_queue(), ^{
                NSDictionary* dict = [NSDictionary dictionaryWithObjectsAndKeys:
                    ns_name, kDiscName,
                    kDiscTypeNames[type], kDiscType,
                    nil];
                [discs setObject:dict forKey:ns_name];
                [ns_name release];
                [sourceList reloadItem:kDiscs reloadChildren:YES];
            });
        },
        .disappeared = ^(rsvc_disc_type_t type, const char* name){
            NSString* ns_name = [[NSString alloc] initWithUTF8String:name];
            dispatch_async(dispatch_get_main_queue(), ^{
                [discs removeObjectForKey:ns_name];
                [ns_name release];
                [sourceList reloadItem:kDiscs reloadChildren:YES];
            });
        },
        .initialized = ^(rsvc_disc_type_t type, const char* name){
        },
    };
    rsvc_disc_watch(callbacks);
}

- (void)dealloc {
    [discs release];
    [super dealloc];
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item {
    if (item == nil) {
        return 1;
    } else if (item == kDiscs) {
        return [discs count];
    } else {
        return 0;
    }
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(id)item {
    if (item == nil) {
        return kDiscs;
    } else if (item == kDiscs) {
        NSArray* keys = [discs allKeys];
        NSArray* sortedKeys = [keys sortedArrayUsingSelector:
            @selector(caseInsensitiveCompare:)];
        return [discs objectForKey:[sortedKeys objectAtIndex:index]];
    }
    return nil;
}

- (NSView*)outlineView:(NSOutlineView*)outlineView
           viewForTableColumn:(NSTableColumn*)tableColumn item:(id)item {
    if ([outlineView parentForItem:item] == nil) {
        NSTextField* field = [outlineView makeViewWithIdentifier:@"HeaderCell" owner:self];
        field.stringValue = item;
        return field;
    } else if ([outlineView parentForItem:item] == kDiscs) {
        NSTableCellView* view = [outlineView makeViewWithIdentifier:@"MainCell" owner:self];
        view.textField.stringValue = [item objectForKey:kDiscName];
        return view;
    }
    return nil;
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

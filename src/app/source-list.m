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
#include "audio-cd-controller.h"

static NSString* kDiscs = @"DISCS";

//static NSString* kDiscName = @"kDiscName";
static NSString* kDiscPath = @"kDiscPath";
static NSString* kDiscType = @"kDiscType";
static NSString* kDiscTypeNames[] = {
    @"CD",
    @"DVD",
    @"Blu-Ray Disc",
};

@implementation RSSourceList

- (void)awakeFromNib {
    [_source_list expandItem:nil expandChildren:YES];
    _discs = [[NSMutableDictionary alloc] init];
    rsvc_disc_watch_callbacks_t callbacks = {
        .appeared = ^(enum rsvc_disc_type type, const char* name){
            NSString* ns_path = [[NSString alloc] initWithUTF8String:name];
            dispatch_async(dispatch_get_main_queue(), ^{
                NSMutableDictionary* disc = [NSMutableDictionary dictionaryWithObjectsAndKeys:
                    ns_path, kDiscPath,
                    kDiscTypeNames[type], kDiscType,
                    nil];

                [_discs setObject:disc forKey:ns_path];
                [ns_path release];
                [_source_list reloadItem:kDiscs reloadChildren:YES];
            });
        },
        .disappeared = ^(enum rsvc_disc_type type, const char* name){
            NSString* ns_path = [[NSString alloc] initWithUTF8String:name];
            dispatch_async(dispatch_get_main_queue(), ^{
                [_discs removeObjectForKey:ns_path];
                [ns_path release];
                [_source_list reloadItem:kDiscs reloadChildren:YES];
            });
        },
        .initialized = ^(rsvc_stop_t stop){
        },
    };
    rsvc_disc_watch(callbacks);
}

- (void)dealloc {
    [_discs release];
    [_view_controller release];
    [super dealloc];
}

- (NSInteger)outlineView:(NSOutlineView*)outlineView numberOfChildrenOfItem:(id)item {
    if (item == nil) {
        return 1;
    } else if (item == kDiscs) {
        return [_discs count];
    } else {
        return 0;
    }
}

- (id)outlineView:(NSOutlineView*)outlineView child:(NSInteger)index ofItem:(id)item {
    if (item == nil) {
        return kDiscs;
    } else if (item == kDiscs) {
        NSArray* keys = [_discs allKeys];
        NSArray* sortedKeys = [keys sortedArrayUsingSelector:
            @selector(caseInsensitiveCompare:)];
        NSMutableDictionary* disc = [_discs objectForKey:[sortedKeys objectAtIndex:index]];
        return disc;
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
        return [outlineView makeViewWithIdentifier:@"MainCell" owner:self];
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

- (void)outlineViewSelectionDidChange:(NSNotification*)notification {
    if ([_source_list selectedRow] == -1) {
        [_view_controller.view removeFromSuperview];
        [_view_controller release];
        _view_controller = nil;
    } else {
        NSMutableDictionary* item = [_source_list itemAtRow:[_source_list selectedRow]];
        _view_controller = [[RSAudioCDController alloc] initWithDisc:item];
        _view_controller.view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        NSRect frame = {{0, 0}, _view.frame.size};
        _view_controller.view.frame = frame;
        [_view addSubview:_view_controller.view];
    }
}

@end

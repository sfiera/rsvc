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

#include "audio-cd-controller.h"

@implementation RSAudioCDController

- (id)initWithDisc:(NSMutableDictionary*)disc {
    if (!(self = [super initWithNibName:@"AudioCDView" bundle:nil])) {
        return nil;
    }
    _disc = [disc retain];
    return self;
}

- (void)dealloc {
    [_disc release];
    [super dealloc];
}

- (NSString*)album {
    return [_disc objectForKey:@"kAlbum"];
}

- (void)setAlbum:(NSString*)album {
    if (album == nil) {
        [_disc removeObjectForKey:@"kAlbum"];
    } else {
        [_disc setObject:album forKey:@"kAlbum"];
    }
}

- (NSString*)artist {
    return [_disc objectForKey:@"kArtist"];
}

- (void)setArtist:(NSString*)artist {
    if (artist == nil) {
        [_disc removeObjectForKey:@"kArtist"];
    } else {
        [_disc setObject:artist forKey:@"kArtist"];
    }
}

- (NSString*)genre {
    return [_disc objectForKey:@"kGenre"];
}

- (void)setGenre:(NSString*)genre {
    if (genre == nil) {
        [_disc removeObjectForKey:@"kGenre"];
    } else {
        [_disc setObject:genre forKey:@"kGenre"];
    }
}

- (IBAction)startRip:(id)sender {
    NSLog(@"-[RSAudioCDController startRip:]");
}

@end

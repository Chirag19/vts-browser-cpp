/**
 * Copyright (c) 2017 Melown Technologies SE
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * *  Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sstream>
#include <cassert>
#include "Map.h"

#import "MapViewController.h"
#import <SpriteKit/SpriteKit.h>
#import <dlfcn.h>

namespace
{
	void *iosGlGetProcAddress(const char *name)
	{
		return dlsym(RTLD_DEFAULT, name);
	}
}

@interface MapViewController ()
{
	vts::renderer::RenderOptions renderOptions;
}

@property (weak, nonatomic) IBOutlet UIView *gestureViewCenter;
@property (weak, nonatomic) IBOutlet UIView *gestureViewBottom;
@property (weak, nonatomic) IBOutlet UIView *gestureViewLeft;
@property (weak, nonatomic) IBOutlet UIView *gestureViewRight;

@end

@implementation MapViewController

- (void)panCenter:(UIPanGestureRecognizer*)recognizer
{
    switch (recognizer.state)
	{
		case UIGestureRecognizerStateEnded:
		case UIGestureRecognizerStateChanged:
		{
			CGPoint p = [recognizer translationInView:_gestureViewCenter];
			[recognizer setTranslation:CGPoint() inView:_gestureViewCenter];
			map->pan({p.x, p.y, 0});
		} break;
		default:
			break;
	}
}

- (void)panBottom:(UIPanGestureRecognizer*)recognizer
{
    switch (recognizer.state)
	{
		case UIGestureRecognizerStateEnded:
		case UIGestureRecognizerStateChanged:
		{
			CGPoint p = [recognizer translationInView:_gestureViewCenter];
			[recognizer setTranslation:CGPoint() inView:_gestureViewCenter];
			map->rotate({1000 * p.x / renderOptions.width, 0, 0});
		} break;
		default:
			break;
	}
}

- (void)panLeft:(UIPanGestureRecognizer*)recognizer
{
    switch (recognizer.state)
	{
		case UIGestureRecognizerStateEnded:
		case UIGestureRecognizerStateChanged:
		{
			CGPoint p = [recognizer translationInView:_gestureViewCenter];
			[recognizer setTranslation:CGPoint() inView:_gestureViewCenter];
			map->rotate({0, 1000 * p.y / renderOptions.height, 0});
		} break;
		default:
			break;
	}
}

- (void)panRight:(UIPanGestureRecognizer*)recognizer
{
    switch (recognizer.state)
	{
		case UIGestureRecognizerStateEnded:
		case UIGestureRecognizerStateChanged:
		{
			CGPoint p = [recognizer translationInView:_gestureViewCenter];
			[recognizer setTranslation:CGPoint() inView:_gestureViewCenter];
			map->zoom(-50 * p.y / renderOptions.height);
		} break;
		default:
			break;
	}
}

- (void)configureView
{
    self.title = _item.name;
    if (map)
    {
        std::string url([_item.url UTF8String]);
        map->setMapConfigPath(url);
    }
}

- (void)setItem:(ConfigItem*)item
{
    _item = item;
    [self configureView];
}

- (void)viewDidLoad
{
    [super viewDidLoad];
    
    // initialize rendering
    
    GLKView *view = (GLKView *)self.view;
    view.context = [[EAGLContext alloc] initWithAPI: kEAGLRenderingAPIOpenGLES3 sharegroup:mapGlSharegroup()];
    [EAGLContext setCurrentContext:view.context];
    
    vts::renderer::loadGlFunctions(&iosGlGetProcAddress);
    vts::renderer::initialize();
    map->renderInitialize();
    
    // initialize gesture recognizers
    
    {
	    // center view
	    assert(_gestureViewCenter);
		UIPanGestureRecognizer *r = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panCenter:)];
        [_gestureViewCenter addGestureRecognizer:r];
    }
    {
	    // bottom view
	    assert(_gestureViewBottom);
		UIPanGestureRecognizer *r = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panBottom:)];
        [_gestureViewBottom addGestureRecognizer:r];
    }
    {
	    // left view
	    assert(_gestureViewLeft);
		UIPanGestureRecognizer *r = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panLeft:)];
        [_gestureViewLeft addGestureRecognizer:r];
    }
    {
	    // right view
	    assert(_gestureViewRight);
		UIPanGestureRecognizer *r = [[UIPanGestureRecognizer alloc] initWithTarget:self action:@selector(panRight:)];
        [_gestureViewRight addGestureRecognizer:r];
    }
    
    [self configureView];
}

- (void)dealloc
{
	map->renderFinalize();
    vts::renderer::finalize();
}

- (void)update
{
    map->renderTickPrepare();
    map->renderTickRender();
}

- (void)glkView:(nonnull GLKView *)view drawInRect:(CGRect)rect
{
    {
        GLint fbo = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
        renderOptions.targetFrameBuffer = fbo;
    }
    
	vts::renderer::render(renderOptions, map->draws(), map->celestialBody());
    
    renderOptions.targetViewportX = rect.origin.x * view.contentScaleFactor;
    renderOptions.targetViewportY = rect.origin.y * view.contentScaleFactor;
    renderOptions.width = rect.size.width * view.contentScaleFactor;
    renderOptions.height = rect.size.height * view.contentScaleFactor;
    map->setWindowSize(renderOptions.width, renderOptions.height);
}

@end

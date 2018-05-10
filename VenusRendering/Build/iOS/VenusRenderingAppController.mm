#import <UIKit/UIKit.h>
#import "UnityAppController.h"

extern "C" void UnityPluginLoad(IUnityInterfaces* pkUnityInterfaces);
extern "C" void UnityPluginUnload();

@interface VenusRenderingAppController : UnityAppController
{

}
- (void)shouldAttachRenderDelegate;
@end

@implementation VenusRenderingAppController
- (void)shouldAttachRenderDelegate;
{
    UnityRegisterRenderingPluginV5(&UnityPluginLoad, UnityPluginUnload);
}
@end

IMPL_APP_CONTROLLER_SUBCLASS(VenusRenderingAppController)

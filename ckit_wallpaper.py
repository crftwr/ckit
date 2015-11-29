from PIL import Image

from ckit import ckitcore
from ckit import ckit_theme

#--------------------------------------------------------------------

class Wallpaper:

    def __init__( self, window ):
        self.window = window
        self.plane = ckitcore.ImagePlane( window, (0,0), (1,1), 4 )
        self.pil_image = None
        self.crop_rect = (0,0,1,1)

    def destroy(self):
        self.plane.destroy()

    def load( self, filename, strength ):
    
        self.pil_image = Image.open(filename)
        self.pil_image = self.pil_image.convert( "RGBA" )
        
        bg = ckit_theme.getColor("bg")
        bgcolor_pil_image = Image.new( "RGBA", self.pil_image.size, (bg[0],bg[1],bg[2],255) ) 

        self.pil_image = Image.blend( self.pil_image, bgcolor_pil_image, (100-strength)/100.0 )

    def copy( self, src_window ):
    
        src_client_rect = src_window.getClientRect()
        src_left, src_top = src_window.clientToScreen( src_client_rect[0], src_client_rect[1] )
        src_right, src_bottom = src_window.clientToScreen( src_client_rect[2], src_client_rect[3] )

        client_rect = self.window.getClientRect()
        left, top = self.window.clientToScreen( client_rect[0], client_rect[1] )
        right, bottom = self.window.clientToScreen( client_rect[2], client_rect[3] )
        
        crop_ratio = [ 
            float( left - src_left ) / ( src_right - src_left ),
            float( top - src_top ) / ( src_bottom - src_top ),
            float( right - src_left ) / ( src_right - src_left ),
            float( bottom - src_top ) / ( src_bottom - src_top )
        ]
        
        crop_rect = [
            int( src_window.wallpaper.crop_rect[0] + (src_window.wallpaper.crop_rect[2]-src_window.wallpaper.crop_rect[0]) * crop_ratio[0] ),
            int( src_window.wallpaper.crop_rect[1] + (src_window.wallpaper.crop_rect[3]-src_window.wallpaper.crop_rect[1]) * crop_ratio[1] ),
            int( src_window.wallpaper.crop_rect[0] + (src_window.wallpaper.crop_rect[2]-src_window.wallpaper.crop_rect[0]) * crop_ratio[2] ),
            int( src_window.wallpaper.crop_rect[1] + (src_window.wallpaper.crop_rect[3]-src_window.wallpaper.crop_rect[1]) * crop_ratio[3] )
        ]
        
        crop_rect[0] = max( crop_rect[0], 0 )
        crop_rect[1] = max( crop_rect[1], 0 )
        crop_rect[2] = min( crop_rect[2], src_window.wallpaper.pil_image.size[0] )
        crop_rect[3] = min( crop_rect[3], src_window.wallpaper.pil_image.size[1] )
        
        self.pil_image = src_window.wallpaper.pil_image.crop(crop_rect)

    def adjust(self):

        client_rect = self.window.getClientRect()
        client_size = ( max(client_rect[2],1), max(client_rect[3],1) )
        
        try:
            wallpaper_ratio = float(self.pil_image.size[0]) / self.pil_image.size[1]
            client_rect_ratio = float(client_size[0]) / client_size[1]

            if wallpaper_ratio > client_rect_ratio:
                crop_width = int(self.pil_image.size[1] * client_rect_ratio)
                self.crop_rect = ( (self.pil_image.size[0]-crop_width)//2, 0, (self.pil_image.size[0]-crop_width)//2+crop_width, self.pil_image.size[1] )
            else:
                crop_height = int(self.pil_image.size[0] / client_rect_ratio)
                self.crop_rect = ( 0, (self.pil_image.size[1]-crop_height)//2, self.pil_image.size[0], (self.pil_image.size[1]-crop_height)//2+crop_height )
        except:
            self.crop_rect = (0,0,self.pil_image.size[0],self.pil_image.size[1])

        cropped_pil_image = self.pil_image.crop(self.crop_rect)
        cropped_scaled_pil_image = cropped_pil_image.resize( client_size )

        img = ckitcore.Image.fromString( cropped_scaled_pil_image.size, cropped_scaled_pil_image.tobytes() )
        self.plane.setSize(client_size)
        self.plane.setImage(img)

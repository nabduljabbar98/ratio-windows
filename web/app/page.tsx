import DesktopDemo from './DesktopDemo';
import wallpapers from './wallpapers.json';
export const dynamic = 'force-dynamic';
export default function Page(){
  const wallpaper=wallpapers.length?wallpapers[Math.floor(Math.random()*wallpapers.length)]:'/os/wallpaper.png';
  return <DesktopDemo initialWallpaper={wallpaper}/>;
}

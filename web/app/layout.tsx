import type { Metadata } from 'next';
import './globals.css';
export const metadata: Metadata = {title:'Ratio — Desktop Demo',description:'Switch between simulated apps and watch Ratio track create versus consume time. An interactive demo of the Mac menu bar app.'};
export default function RootLayout({children}:{children:React.ReactNode}) {return <html lang="en"><body>{children}</body></html>}

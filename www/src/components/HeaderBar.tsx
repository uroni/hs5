import logoImage from '../assets/hs5.png'
import { useSnapshot } from 'valtio';
import { Pages, router, state } from '../App';
import { useStackStyles } from './StackStyles';
import { Avatar, Image, Menu, MenuList, MenuItem, MenuTrigger, MenuPopover, MenuButton } from '@fluentui/react-components';
import { logout } from '../api';
import { startTransition } from 'react';
import { KeyRegular, PowerRegular } from '@fluentui/react-icons';

function navigateToChangePassword() {
  startTransition(() => {
    router.navigate(`/${Pages.ChangePassword}`);
  });
}

export const HeaderBar = () => {

  const snap = useSnapshot(state);
  const styles = useStackStyles();

  const handleLogout = async () => {
    state.loggingOut = true;
    await logout();
    state.loggedIn = false;
    state.session = "";
    state.accessKey = "";
    state.secretAccessKey = "";
    if (window.localStorage) {
      localStorage.removeItem("ses");
      localStorage.removeItem("accessKey");
      localStorage.removeItem("secretAccessKey");
    }
    startTransition(() => {
        router.navigate(`/`);
      });
    state.loggingOut = false;
  }


  return (
    <div className={styles.stackHorizontal} style={{alignItems: "center"}}>
        <div className={styles.item} style={{padding: "5px"}}>
          <Image src={logoImage} fit='contain' />
        </div>
        <div className={styles.item}>
          HS5
        </div>
        <div className={styles.itemGrow}>

        </div>
        {snap.loggedIn && 
        <div style={{paddingRight: "10px"}}>
          <Menu positioning={{ autoSize: true }}>
            <MenuTrigger disableButtonEnhancement>
              <MenuButton appearance="transparent"><Avatar aria-label='guest' /></MenuButton>
            </MenuTrigger>
            <MenuPopover>
              <MenuList>
                <MenuItem onClick={() => navigateToChangePassword()} icon={<KeyRegular />}>Change password</MenuItem>
                <MenuItem onClick={() => handleLogout()} icon={<PowerRegular />}>Logout</MenuItem>                
              </MenuList>
            </MenuPopover>
          </Menu>
        </div>
      }
      </div>
  );
}

export default HeaderBar;
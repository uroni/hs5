import logoImage from '../assets/hs5.png'
import { useSnapshot } from 'valtio';
import { Pages, router, state } from '../App';
import { useStackStyles } from './StackStyles';
import { Avatar, Image, Menu, MenuList, MenuItem, MenuTrigger, MenuPopover, MenuButton } from '@fluentui/react-components';
import { postApiV1B64Be5124B034028A58913931942E205Logout } from '../api';
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
    await postApiV1B64Be5124B034028A58913931942E205Logout({requestBody: {ses: snap.session}});
    state.loggedIn = false;
    state.session = "";
    state.accessKey = "";
    state.secretAccessKey = "";
    startTransition(() => {
        router.navigate(`/`);
      });
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
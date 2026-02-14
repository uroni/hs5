import { FormEvent, Suspense, useState } from 'react';
import { state } from '../App';
import { Field, Button, Input, Spinner, MessageBar, MessageBarBody, MessageBarTitle } from '@fluentui/react-components';
import { useSnapshot } from 'valtio';
import { ApiError, postApiV1B64Be5124B034028A58913931942E205ChangePassword } from '../api';
import { HapiError, Herror } from '../errorapi/HapiError';
import { TableWrapper } from '../components/TableWrapper';

const ChangePassword = () => {
  const [oldPassword, setOldPassword] = useState('');
  const [newPassword, setNewPassword] = useState('');
  const [confirmPassword, setConfirmPassword] = useState('');
  const [isLoading, setLoading] = useState(false);
  const [oldPasswordError, setOldPasswordError] = useState('');
  const [newPasswordError, setNewPasswordError] = useState('');
  const [confirmPasswordError, setConfirmPasswordError] = useState('');
  const [successMessage, setSuccessMessage] = useState('');
  const [generalError, setGeneralError] = useState('');

  const snap = useSnapshot(state);

  const clearErrors = () => {
    setOldPasswordError('');
    setNewPasswordError('');
    setConfirmPasswordError('');
    setGeneralError('');
    setSuccessMessage('');
  };

  const handleSubmitInt = async () => {
    clearErrors();

    if (!oldPassword) {
      setOldPasswordError('Current password is required');
      return;
    }

    if (!newPassword) {
      setNewPasswordError('New password is required');
      return;
    }

    if (newPassword !== confirmPassword) {
      setConfirmPasswordError('Passwords do not match');
      return;
    }

    try {
      await postApiV1B64Be5124B034028A58913931942E205ChangePassword({
        requestBody: {
          ses: snap.session,
          oldPassword: oldPassword,
          password: newPassword
        }
      });
      setSuccessMessage('Password changed successfully!');
      setOldPassword('');
      setNewPassword('');
      setConfirmPassword('');
    } catch (apiE) {
      if (apiE instanceof ApiError) {
        const e = apiE.body as HapiError;
        switch (e.herror) {
          case Herror.PasswordWrong:
            setOldPasswordError('Current password is incorrect');
            break;
          default:
            setGeneralError(e.msg || 'Failed to change password');
        }
      } else {
        setGeneralError('Failed to change password');
      }
    }
  };

  const handleSubmit = async (e: FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    setLoading(true);
    try {
      await handleSubmitInt();
    } finally {
      setLoading(false);
    }
  };

  return (
    <Suspense fallback={<Spinner />}>
      <TableWrapper>
        <h3>Change Password</h3>
        <div style={{ maxWidth: '400px' }}>
          <form onSubmit={handleSubmit}>
            {successMessage && (
              <MessageBar intent="success" style={{ marginBottom: '1rem' }}>
                <MessageBarBody>
                  <MessageBarTitle>Success</MessageBarTitle>
                  {successMessage}
                </MessageBarBody>
              </MessageBar>
            )}
            {generalError && (
              <MessageBar intent="error" style={{ marginBottom: '1rem' }}>
                <MessageBarBody>
                  <MessageBarTitle>Error</MessageBarTitle>
                  {generalError}
                </MessageBarBody>
              </MessageBar>
            )}
            <Field
              label="Current Password"
              required
              validationMessage={oldPasswordError}
              validationState={oldPasswordError ? 'error' : 'none'}
            >
              <Input
                type="password"
                value={oldPassword}
                onChange={(e) => setOldPassword(e.target.value)}
              />
            </Field>
            <Field
              label="New Password"
              required
              validationMessage={newPasswordError}
              validationState={newPasswordError ? 'error' : 'none'}
              style={{ marginTop: '1rem' }}
            >
              <Input
                type="password"
                value={newPassword}
                onChange={(e) => setNewPassword(e.target.value)}
              />
            </Field>
            <Field
              label="Confirm New Password"
              required
              validationMessage={confirmPasswordError}
              validationState={confirmPasswordError ? 'error' : 'none'}
              style={{ marginTop: '1rem' }}
            >
              <Input
                type="password"
                value={confirmPassword}
                onChange={(e) => setConfirmPassword(e.target.value)}
              />
            </Field>
            <div style={{ marginTop: '1rem' }}>
              {isLoading && <Spinner label="Changing password..." />}
              {!isLoading && <Button type="submit" appearance="primary">Change Password</Button>}
            </div>
          </form>
        </div>
      </TableWrapper>
    </Suspense>
  );
};

export default ChangePassword;
